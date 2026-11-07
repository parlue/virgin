// Included inside uci.cpp's private namespace, after findOption().
// Only snapshot publication is locked; never hold this mutex across search.
bool g_livePerformance = false;
std::atomic<bool> g_commandCancel{false};
bool g_prepareNext = false;
struct PerformanceMove { Position before; Move move; size_t index; };
std::mutex g_performanceMutex;
std::array<virgin_performance::Performance,2> g_performance;
std::array<unsigned,2> g_performanceSamples{}, g_performancePly{};
std::deque<PerformanceMove> g_performancePending;
std::vector<Move> g_performancePrefix;
std::string g_performanceBase, g_performanceCacheFen;
std::vector<SearchCandidate> g_performanceCache;
size_t g_performanceCommitted = 0, g_performanceSkipped = 0;
uint64_t g_performanceRevision = 0;
bool performanceActive() { return g_adaptive || g_livePerformance; }
void resetPerformance() {
    std::lock_guard<std::mutex> lock(g_performanceMutex);
    for (auto& p:g_performance) p.reset();
    g_performanceSamples={}; g_performancePly={}; g_performancePending.clear();
    g_performancePrefix.clear(); g_performanceBase.clear();
    g_performanceCache.clear(); g_performanceCacheFen.clear();
    g_performanceCommitted=g_performanceSkipped=0; ++g_performanceRevision;
}
void stagePerformance(const Position& base, const std::vector<Move>& moves) {
    if (!performanceActive()) return;
    std::lock_guard<std::mutex> lock(g_performanceMutex);
    const auto fen=base.fen(); size_t common=0;
    if (fen==g_performanceBase)
        while (common<moves.size() && common<g_performancePrefix.size() && moves[common]==g_performancePrefix[common]) ++common;
    const bool reset=fen!=g_performanceBase || common<g_performanceCommitted;
    if (reset) {
        for(auto& p:g_performance)p.reset();
        g_performanceSamples={};g_performancePly={};g_performanceCommitted=0;
        g_performanceCache.clear();g_performanceCacheFen.clear();g_performanceSkipped=0;
    }
    if (reset || common!=moves.size() || moves.size()!=g_performancePrefix.size()) ++g_performanceRevision;
    g_performanceBase=fen;g_performancePrefix=moves;g_performancePending.clear();
    // Large imports/late activation warm-start from six halfmoves rather than
    // silently spending a whole game worth of CPU before replying to go.
    if(moves.size()>g_performanceCommitted+6) {
        g_performanceSkipped+=moves.size()-6-g_performanceCommitted;
        g_performanceCommitted=moves.size()-6;
    }
    Position board=base;
    for(size_t i=0;i<moves.size();++i) {
        if(i>=g_performanceCommitted)g_performancePending.push_back({board,moves[i],i});
        board.applyMove(moves[i]);
    }
}
void restartPerformanceProfile() {
    std::string fen;std::vector<Move> moves;
    {std::lock_guard<std::mutex> lock(g_performanceMutex);fen=g_performanceBase;moves=g_performancePrefix;}
    resetPerformance();
    Position base;std::string error;
    if(!fen.empty() && base.setFen(fen,&error))stagePerformance(base,moves);
}
void reportPerformance() {
    std::ostringstream response;
    {
        std::lock_guard<std::mutex> lock(g_performanceMutex);
        const auto w=g_performance[WHITE].snapshot(), b=g_performance[BLACK].snapshot();
        auto label=[](const auto& p){return p.above?std::string(">2800"):std::to_string(p.rounded);};
        response<<"info string performance white "<<label(w)<<" black "<<label(b)<<" window 3 active "
            <<(performanceActive()?"true":"false")<<" white_samples "<<g_performanceSamples[WHITE]
            <<" black_samples "<<g_performanceSamples[BLACK]<<" white_ply "<<g_performancePly[WHITE]
            <<" black_ply "<<g_performancePly[BLACK]<<" pending "<<g_performancePending.size()
            <<" revision "<<g_performanceRevision<<" skipped "<<g_performanceSkipped
            <<" white_lossguard "<<(w.lossGuard?"true":"false")<<" black_lossguard "<<(b.lossGuard?"true":"false")
            <<" status "<<(!performanceActive()?"disabled":!g_performancePending.empty()?"pending":
                g_performanceSamples[WHITE]+g_performanceSamples[BLACK]==0?"initial":"ready");
    }
    const UciOutputGuard outputGuard;uciOutput()<<response.str()<<std::endl;
}
struct PerformanceSink : std::streambuf {
    int_type overflow(int_type ch) override { return traits_type::not_eof(ch); }
    std::streamsize xsputn(const char*,std::streamsize n) override { return n; }
};
bool fullPerformanceProfile() {
    if(g_strength.maxDepth<64 || g_strength.nodeLimit || g_strength.evalNoise || g_strength.moveNoise || g_strength.blunderPct)return false;
    return std::all_of(std::begin(EW.knowledge),std::end(EW.knowledge),[](int n){return n==100;});
}
void cachePerformance(const Position& pos,const SearchResult& r) {
    if(!performanceActive() || !r.best || r.depth<1 || !fullPerformanceProfile())return;
    std::lock_guard<std::mutex> lock(g_performanceMutex);
    g_performanceCacheFen=pos.fen();g_performanceCache=r.candidates;
    if(g_performanceCache.empty())g_performanceCache.push_back({r.best,r.score,r.pv});
}
void measurePerformance(int totalMs,bool speculativePonder) {
    if(!performanceActive() || totalMs<=0)return;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(totalMs);
    PerformanceSink buffer;std::ostream sink(&buffer);
    while(!g_commandCancel.load()) {
        std::optional<PerformanceMove> pending;size_t workCount=1;
        std::vector<SearchCandidate> cached;
        {
            std::lock_guard<std::mutex> lock(g_performanceMutex);
            if(g_performancePending.empty())break;
            const auto& front=g_performancePending.front();
            if(speculativePonder && front.index+1==g_performancePrefix.size())break;
            pending=front;
            workCount=g_performancePending.size();
            if(speculativePonder && workCount>1)--workCount;
            if(g_performanceCacheFen==front.before.fen())cached=g_performanceCache;
        }
        const auto& item=*pending;
        SearchResult joint; joint.candidates=cached;
        auto containsPlayed=[&]{return std::any_of(joint.candidates.begin(),joint.candidates.end(),[&](const auto& c){return c.move==item.move;});};
        if(!containsPlayed()) {
            const int remaining=int(std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count());
            if(remaining<8)break;
            const int slice=std::max(4,remaining/int(std::max<size_t>(1,workCount)));
            const auto sliceEnd=std::chrono::steady_clock::now()+std::chrono::milliseconds(slice);
            const Strength saved=g_strength; const int threads=threadCount();
            std::array<int,GROUP_NB> knowledge;std::copy_n(EW.knowledge,GROUP_NB,knowledge.begin());
            const bool changed=!fullPerformanceProfile();clearStrength();setThreads(1);if(changed)TT.clear();
            const size_t count=std::min(item.index+1,g_gameKeys.size());
            std::vector<Key> history(g_gameKeys.begin(),g_gameKeys.begin()+count);
            SearchLimits analysis;analysis.depth=8;analysis.multiPV=3;analysis.moveOverhead=0;
            analysis.cancellation=&g_commandCancel;
            if(cached.empty()) {
                analysis.movetime=std::max(2,slice/3);
                joint=searchPosition(item.before,analysis,history,sink);
                cached=joint.candidates;
            }
            if(!g_commandCancel.load() && !cached.empty()) {
                analysis.restrictRootMoves=true;
                for(size_t i=0;i<std::min<size_t>(3,cached.size());++i)analysis.rootMoves.push_back(cached[i].move);
                if(std::find(analysis.rootMoves.begin(),analysis.rootMoves.end(),item.move)==analysis.rootMoves.end())analysis.rootMoves.push_back(item.move);
                analysis.multiPV=int(analysis.rootMoves.size());
                analysis.movetime=std::max(2,int(std::chrono::duration_cast<std::chrono::milliseconds>(sliceEnd-std::chrono::steady_clock::now()).count()));
                joint=searchPosition(item.before,analysis,history,sink);
            } else joint.candidates.clear();
            g_strength=saved;std::copy(knowledge.begin(),knowledge.end(),EW.knowledge);setThreads(threads);if(changed)TT.clear();
            if(g_commandCancel.load() || joint.depth<1 || !containsPlayed())break;
        }
        if(joint.candidates.empty() || !containsPlayed())break;
        Score best=joint.candidates.front().score, played=best;
        for(const auto& c:joint.candidates) {best=std::max(best,c.score);if(c.move==item.move)played=c.score;}
        // Search mates have large signed scores; the mathematical transform
        // itself caps these at +/-1000, preserving win/loss rather than distance.
        int queens=0,other=0;
        for(int sq=0;sq<64;++sq) {const Piece pc=item.before.pieceOn(Square(sq));if(!is_ok(pc))continue;
            const auto pt=type_of(pc);queens+=pt==QUEEN;other+=pt==ROOK||pt==BISHOP||pt==KNIGHT;}
        const double loss=virgin_performance::penalty(best,played,queens,other);
        std::lock_guard<std::mutex> lock(g_performanceMutex);
        const Color color=item.before.sideToMove();
        g_performance[color].add(loss,double(played));++g_performanceSamples[color];
        g_performancePly[color]=unsigned(item.index+1);g_performanceCommitted=item.index+1;
        g_performancePending.pop_front();++g_performanceRevision;
    }
}
