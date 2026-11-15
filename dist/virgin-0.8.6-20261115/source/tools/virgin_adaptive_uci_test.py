import argparse, subprocess, threading, queue, time, re, pathlib, json
import chess
class Engine:
    def __init__(self,path):
        self.p=subprocess.Popen([path],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        self.q=queue.Queue();self.log=[]
        def reader():
            for line in self.p.stdout:self.q.put(line.rstrip())
            self.q.put(None)
        threading.Thread(target=reader,daemon=True).start()
    def send(self,line):self.log.append('> '+line);self.p.stdin.write(line+'\n');self.p.stdin.flush()
    def until(self,prefix,timeout=20):
        lines=[];end=time.monotonic()+timeout
        while True:
            line=self.q.get(timeout=max(.001,end-time.monotonic()));assert line is not None,'process ended'
            self.log.append(line);lines.append(line)
            if line.startswith(prefix):return lines
    def sync(self,*commands):
        for c in commands:self.send(c)
        self.send('isready');return self.until('readyok')
    def search(self,go):self.send(go);return self.until('bestmove ')
    def close(self):
        if self.p.poll() is None:
            self.send('quit')
            try:self.p.wait(timeout=5)
            except subprocess.TimeoutExpired:self.p.kill();self.p.wait()
def weights(lines):return [l for l in lines if ' = ' in l or l.startswith('info string eval') or l.startswith('info string terms:')]
def diag(lines):return next(l for l in lines if l.startswith('info string adaptive ') and ' samples ' in l)
def verify(lines,board):
    candidates={};depths={}
    for l in lines:
        if l.startswith('info depth ') and ' pv ' in l:
            t=l.split();pv=t[t.index('pv')+1:];b=board.copy()
            for u in pv:
                m=chess.Move.from_uci(u);assert m in b.legal_moves,(board.fen(),l);b.push(m)
            if 'multipv' in t:
                depth=int(t[t.index('depth')+1]);rank=int(t[t.index('multipv')+1]);depths.setdefault(depth,{})[rank]=pv[0]
    for depth,batch in depths.items():
        assert len(batch)==min(5,board.legal_moves.count()),(depth,batch)
        assert len(set(batch.values()))==len(batch)
    best=chess.Move.from_uci(lines[-1].split()[1]);assert best in board.legal_moves
    return best,max(depths,default=0)
def main(args):
    e=Engine(args.engine);passed=[]
    try:
        e.send('uci');hello=e.until('uciok');assert 'option name Adaptive type check default false' in hello
        passed.append('Adaptive advertised as independent check option')
        before=e.sync('setoption name Threads value 1','setoption name Hash value 1',
                      'setoption name Style value Furchtsam','setoption name Material.Pawn.Mg value 150',
                      'setoption name UCI_Elo value 700','setoption name UCI_LimitStrength value true','weights','eval')
        active=e.sync('setoption name Adaptive value true','weights','eval')
        ignored=['Hash value 128','Threads value 64','UCI_Elo value 700','UCI_LimitStrength value true',
                 'Style value Furchtsam','Boldness value 10','Bot value v700-furchtsam',
                 'WeightsFile value Z:/missing/weights.txt','Material.Pawn.Mg value 900','ResetWeights','ReloadWeights','Clear Hash','Ponder value true']
        after=e.sync(*['setoption name '+s for s in ignored],'weights','eval')
        assert weights(active)==weights(after)
        assert sum('Adaptive active: ignored option' in l for l in after)==len(ignored)
        passed.append('All conflicting options ignored; effective weights and evaluation unchanged')
        board=chess.Board();e.sync('ucinewgame','position startpos')
        first=e.search('go depth 4');best,depth=verify(first,board);assert depth==4
        assert 'adaptive eroeffnung ' in diag(first) and 'samples 0' in diag(first)
        passed.append('Five complete legal same-depth candidates, full speed despite Elo 700 settings')
        repeat=e.search('go depth 4');verify(repeat,board);assert 'samples 0' in diag(repeat)
        passed.append('Repeated analysis does not fabricate a player accuracy sample')
        best=chess.Move.from_uci(repeat[-1].split()[1]);board.push(best);reply=list(board.legal_moves)[0];board.push(reply)
        e.sync('position startpos moves '+best.uci()+' '+reply.uci())
        continuation=e.search('go depth 3');verify(continuation,board);assert 'samples 1' in diag(continuation)
        e.sync('ucinewgame','position startpos');fresh=e.search('go depth 3');verify(fresh,chess.Board());assert 'samples 0' in diag(fresh)
        passed.append('Player move measured once; ucinewgame resets the complete adaptive state')
        e.send('go infinite');e.until('info depth 2 ')
        e.send('setoption name UCI_Elo value 700');ignored_live=e.until('info string Adaptive active: ignored option')
        e.send('isready');ready=e.until('readyok');assert not any(l.startswith('bestmove') for l in ignored_live+ready)
        e.send('stop');stopped=e.until('bestmove ');assert chess.Move.from_uci(stopped[-1].split()[1]) in chess.Board().legal_moves
        passed.append('Ignored options do not stop a running adaptive search; stop stays responsive')
        runtime=[]
        fens=[chess.STARTING_FEN,'r2q1rk1/pp3ppp/2pp1n2/2b1p1B1/2B1P3/2NP1N2/PPP2PPP/R2Q1RK1 w - - 4 8']
        for fen in fens:
            position=chess.Board(fen);assert position.is_valid()
            e.sync('ucinewgame','position fen '+fen)
            started=time.monotonic();timed=e.search('go movetime 1000');elapsed=round((time.monotonic()-started)*1000)
            verify(timed,position);assert elapsed<1500,(elapsed,timed[-4:])
            runtime.append(elapsed)
        passed.append('One-second adaptive searches stay within time: '+str(runtime)+' ms; every PV legal')
        single=None
        for queen in chess.SQUARES:
            if queen in [chess.F6,chess.H8]:continue
            b=chess.Board(None);b.set_piece_at(chess.F6,chess.Piece(chess.KING,chess.WHITE));b.set_piece_at(chess.H8,chess.Piece(chess.KING,chess.BLACK))
            b.set_piece_at(queen,chess.Piece(chess.QUEEN,chess.WHITE));b.turn=chess.BLACK
            if b.is_valid() and b.legal_moves.count()==1:single=b;break
        assert single is not None
        e.sync('ucinewgame','position fen '+single.fen());verify(e.search('go depth 3'),single)
        passed.append('A single legal root move forms a valid one-candidate batch')
        restored=e.sync('setoption name Adaptive value false','position startpos','weights','eval')
        assert weights(restored)==weights(before)
        limited=e.search('go depth 7');depths=[int(l.split()[2]) for l in limited if l.startswith('info depth ')]
        assert max(depths)<=2,(depths,limited)
        passed.append('Disabling restores weights, knowledge and the prior Elo handicap')
        e.sync('setoption name Adaptive value true','ucinewgame','position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1')
        terminal=e.search('go movetime 20');assert terminal[-1]=='bestmove 0000'
        passed.append('Terminal positions return 0000 safely')
    finally:
        e.close();pathlib.Path(args.log).write_text('\n'.join(e.log)+'\n',encoding='utf-8')
    pathlib.Path(args.out).write_text(json.dumps(dict(passed=passed),indent=2),encoding='utf-8')
    for item in passed:print('PASS',item)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('engine');p.add_argument('--out',required=True);p.add_argument('--log',required=True);main(p.parse_args())
