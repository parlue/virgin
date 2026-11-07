#include "virgin_native.h"
#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "tt.h"
#include "uci.h"
#include "weights.h"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <deque>
#include <istream>
#include <mutex>
#include <ostream>
#include <streambuf>
#include <string>
#include <thread>

namespace {
std::atomic<bool> occupied{false};
std::once_flag initialized;
struct Session {
    std::mutex mutex;
    std::condition_variable inputReady, outputReady;
    std::deque<std::string> commands, lines;
    bool closing=false, done=false;
    std::thread worker;
    struct Input : std::streambuf {
        Session& s; std::string current;
        explicit Input(Session& owner):s(owner) {}
        int_type underflow() override {
            if (gptr() && gptr()<egptr()) return traits_type::to_int_type(*gptr());
            std::unique_lock<std::mutex> lock(s.mutex);
            s.inputReady.wait(lock,[&]{return !s.commands.empty() || s.closing;});
            if (s.commands.empty()) return traits_type::eof();
            current=std::move(s.commands.front());s.commands.pop_front();current+='\n';
            setg(current.data(),current.data(),current.data()+current.size());
            return traits_type::to_int_type(*gptr());
        }
    } input{*this};
    struct Output : std::streambuf {
        Session& s; std::string current;
        explicit Output(Session& owner):s(owner) {}
        int_type overflow(int_type ch) override {
            if (traits_type::eq_int_type(ch,traits_type::eof())) return traits_type::not_eof(ch);
            char c=traits_type::to_char_type(ch);
            if(c=='\n') {
                if(!current.empty() && current.back()=='\r') current.pop_back();
                s.emit(std::move(current));current.clear();
            } else current+=c;
            return ch;
        }
        int sync() override { return 0; }
    } output{*this};
    void emit(std::string text) {
        std::lock_guard<std::mutex> lock(mutex); lines.push_back(std::move(text));outputReady.notify_all();
    }
    void run() noexcept {
        try {
            std::call_once(initialized,[]{init_bitboards();init_zobrist();init_weights();initSearch();});
            uciResetSession();
            std::istream in(&input);std::ostream out(&output);
            uciRun(in,out);
        } catch (...) { uciShutdown();emit("info string native session failed"); }
        // No search worker may reference the output stream once done is set.
        TT.release();
        std::lock_guard<std::mutex> lock(mutex);done=true;outputReady.notify_all();
    }
};
}
extern "C" VIRGIN_API void* virgin_create() {
    bool expected=false;
    if(!occupied.compare_exchange_strong(expected,true)) return nullptr;
    Session* s=nullptr;
    try { s=new Session;s->worker=std::thread([s]{s->run();});return s; }
    catch (...) {delete s;occupied=false;return nullptr;}
}
extern "C" VIRGIN_API int virgin_send(void* handle,const char* command) {
    if(!handle || !command) return -1;
    try {
        std::string text(command);
        if(text.empty() || text.size()>65536 || text.find_first_of("\r\n")!=std::string::npos) return -1;
        auto& s=*static_cast<Session*>(handle);std::lock_guard<std::mutex> lock(s.mutex);
        if(s.closing || s.done) return 0;
        s.commands.push_back(std::move(text));s.inputReady.notify_one();return 1;
    } catch (...) {return -1;}
}
extern "C" VIRGIN_API int virgin_read(void* handle,char* buffer,int capacity,int timeout) {
    if(!handle || !buffer || capacity<1 || timeout<0) return -1;
    auto& s=*static_cast<Session*>(handle);std::unique_lock<std::mutex> lock(s.mutex);
    if(!s.outputReady.wait_for(lock,std::chrono::milliseconds(timeout),[&]{return !s.lines.empty() || s.done;})) return -2;
    if(s.lines.empty()) return 0;
    const auto& line=s.lines.front();if(line.size()>=static_cast<size_t>(capacity)) return -3;
    std::memcpy(buffer,line.c_str(),line.size()+1);s.lines.pop_front();return 1;
}
extern "C" VIRGIN_API void virgin_destroy(void* handle) {
    if(!handle) return;
    auto* s=static_cast<Session*>(handle);
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        s->commands.clear();s->commands.push_back("quit");s->closing=true;s->inputReady.notify_all();
    }
    stopSearch();
    if(s->worker.joinable()) s->worker.join();
    delete s;occupied.store(false);
}
