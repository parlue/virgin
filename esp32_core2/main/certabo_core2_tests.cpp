#include "certabo_session.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace core2;
static int checks = 0;
static void check(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
  ++checks;
}
static certabo::RawBoard setup(bool rgb) {
  constexpr char start[] = "rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR";
  certabo::RawBoard raw{};
  for (int i=0;i<64;++i)
    if (start[i]!='.') raw[i] = {93,0,21,uint8_t(i+1),42};
  raw[rgb?20:19] = {93,0,21,90,42};
  raw[rgb?44:43] = {93,0,21,91,42};
  return raw;
}
static std::string wire(const certabo::RawBoard &raw) {
  std::string out=":";
  for (const auto &id:raw) for (auto byte:id) out+=std::to_string(byte)+" ";
  return out+"\r\n";
}
static bool feed(CertaboSession &session, const certabo::RawBoard &raw, Board &out) {
  bool got=false;
  for (auto byte:wire(raw)) got=session.feed(uint8_t(byte),out)||got;
  return got;
}
int main() {
  try {
    constexpr char start[] = "RNBQKBNRPPPPPPPP................................pppppppprnbqkbnr";
    check(certaboName("Certabo abc") && certaboName("Tabutronic Spectrum") &&
          !certaboName("Chessnut GO") && supportedName("CERTABO"), "discovery");
    const std::array<uint8_t,3> colors[] = {{140,0,0},{240,90,10},{70,180,255},{40,190,150},{46,204,113}};
    for (int i=0;i<5;++i) check(spectrumMoodColour(-400+i*200)==colors[i], "chesstactics colour stops");
    check(spectrumMoodColour(-99999)==colors[0] && spectrumMoodColour(99999)==colors[4], "mate saturation");
    check(spectrumTint(colors[2])==std::array<uint8_t,3>{7,35,255} &&
          spectrumTint(colors[0])==colors[0] && spectrumTint(colors[4])==colors[4],
          "chesstactics Spectrum tint makes neutral LEDs blue and preserves red/green");
    check(spectrumMoodColour(-100)==std::array<uint8_t,3>{155,135,132} &&
          spectrumMoodColour(100)==std::array<uint8_t,3>{55,185,202}, "continuous interpolation");
    for (bool rgb:{false,true}) {
      CertaboSession session; session.reset(); Board board{};
      auto raw=setup(rgb);
      check(session.leds(~uint64_t(0),0,0).empty(), "unknown model sends no LEDs");
      check(session.clearLeds().empty(), "unknown model cannot send clear probe");
      // A contradictory L/D marker and identical ID prefixes cannot select type.
      session.feed(uint8_t(rgb?'L':'D'),board);
      for (int i=0;i<9;++i) check(!feed(session,raw,board), "calibration cannot publish extra queens");
      check(session.calibrated() && !session.ready() && session.spectrum()==rgb, "queen layout selects driver and 34 IDs");
      const auto off=session.clearLeds();
      check(off.size()==(rgb?247u:8u),"disconnect can clear selected model during startup");
      if(rgb) check(std::all_of(off.begin()+2,off.begin()+245,[](uint8_t x){return x==0;}),"Spectrum clear has no lit corners");
      for (unsigned phase=0;phase<6;++phase) {
        auto command=session.leds(0,400,phase*700);
        check(command.size()==(rgb?247u:8u), "correct startup LED format");
        if (rgb) {
          bool lit=false;
          for (int i=0;i<81;++i) {
            check(command[2+i*3]==0 && command[3+i*3]==0, "setup uses blue regardless of eval");
            if(command[4+i*3])lit=true;
          }
          check(lit==(phase%2==0), "three blue confirmation flashes");
        }
        session.sent(phase*700);
      }
      session.leds(0,0,4200);
      raw[rgb?20:19]={};
      check(!feed(session,raw,board), "one spare queen still blocks play");
      raw[rgb?44:43]={};
      check(feed(session,raw,board) && session.ready() && std::string(board.begin(),board.end())==start, "removing queens publishes canonical starting position");
      auto command=session.leds(uint64_t(1)<<12,0,4400);
      if(rgb) {
        check(command[0]==255 && command[1]==85 && command[245]==13 && command[246]==10, "Spectrum framing");
        int lit=0;
        for(int i=0;i<81;++i)if(command[2+3*i]||command[3+3*i]||command[4+3*i]) {
          ++lit; check(command[2+3*i]==7 && command[3+3*i]==35 && command[4+3*i]==255,"neutral play is blue with Spectrum hardware tint");
        }
        check(lit==4,"one square lights four Spectrum corners");
        check(session.leds(uint64_t(1)<<12,400,4400)!=command,"same mask changes with evaluation");
      } else {
        check(command[6]==16,"mono e2 mask orientation");
        check(session.leds(uint64_t(1)<<12,400,4400)==command,"mono ignores evaluation");
      }
      // Parser raw order e2=52, e4=36.
      const auto pawn=raw[52];raw[52]={};
      check(feed(session,raw,board) && board[12]=='.',"lift is canonical");
      raw[36]=pawn;
      check(feed(session,raw,board) && board[28]=='P',"e2-e4 decoded");
      auto invalid=raw;invalid[35]=pawn;
      check(!feed(session,invalid,board),"duplicate IDs withhold complete position");
      invalid=raw;invalid[35]={7,7,7,7,7};
      check(!feed(session,invalid,board),"unknown ID never becomes empty square");
      auto partial=wire(raw);
      for(size_t i=0;i<partial.size()/2;++i)session.feed(uint8_t(partial[i]),board);
      session.gap();
      bool got=false;
      for(size_t i=partial.size()/2;i<partial.size();++i)got=session.feed(uint8_t(partial[i]),board)||got;
      check(!got && feed(session,raw,board),"queue gap discards partial frame and next full frame recovers");
      session.reset();
      check(!session.ready() && !session.calibrated() && session.type()==certabo::LedType::Unknown,"reconnect requires fresh queen choice");
    }
    std::cout<<"PASS Certabo Core2: "<<checks<<" checks\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
