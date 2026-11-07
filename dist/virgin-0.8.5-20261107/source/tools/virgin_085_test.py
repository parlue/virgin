"""Release-specific UCI integration checks; requires python-chess."""
import argparse,json,sys,time,queue
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from virgin_adaptive_uci_test import Engine
import chess

def main(a):
 e=Engine(a.engine); results=[]
 def record(name,**extra):
  results.append(dict(case=name,**extra));print('PASS',name,extra,flush=True)
 def search(board,command,ceiling):
  start=time.monotonic(); lines=e.search(command); ms=round((time.monotonic()-start)*1000)
  best=chess.Move.from_uci(lines[-1].split()[1]);assert best in board.legal_moves,(command,lines[-10:])
  assert ms<=ceiling,(command,ms,ceiling,lines[-8:])
  for line in lines:
   if line.startswith('info depth ') and ' pv ' in line:
    copy=board.copy()
    for text in line.split(' pv ',1)[1].split():
     move=chess.Move.from_uci(text);assert move in copy.legal_moves,(line,text);copy.push(move)
  primary=[line for line in lines if line.startswith('info depth ') and ' pv ' in line and ' multipv ' not in line]
  if any(' multipv ' in line for line in lines) and not any('info string adaptive ' in line for line in lines):
   assert primary and primary[-1].split(' pv ',1)[1].split()[0]==best.uci(),lines[-8:]
  return ms,lines,best.uci()
 try:
  e.send('uci');hello=e.until('uciok');assert 'id name Virgin 0.8.5' in hello
  assert any(l.startswith('option name Move Overhead type spin') for l in hello)
  record('0.8.5 UCI identity and timing safety option')
  board=chess.Board();board.push_uci('e2e4')
  for adaptive in [False,True]:
   e.sync('setoption name Adaptive value '+str(adaptive).lower(),'ucinewgame','position startpos moves e2e4')
   ms,lines,best=search(board,'go',10500 if adaptive else 5500)
   record('automatic play budget',adaptive=adaptive,ms=ms,best=best)
   e.sync('ucinewgame','position startpos moves e2e4')
   ms,lines,best=search(board,'go movetime 3000',3300)
   record('explicit three seconds overrides automatic budget',adaptive=adaptive,ms=ms,best=best)
   e.sync('ucinewgame','position startpos moves e2e4')
   ms,lines,best=search(board,'go wtime 300000 btime 300000 winc 2000 binc 2000',3400)
   record('5+2 clock keeps opening quick',adaptive=adaptive,ms=ms,best=best)
   for command in ['go wtime 0 btime 0','go wtime -1 btime -1','go movetime 0','go nodes 0']:
    e.sync('ucinewgame','position startpos moves e2e4')
    ms,lines,best=search(board,command,700)
    record('empty/negative explicit limits remain bounded',adaptive=adaptive,command=command,ms=ms)
  e.sync('setoption name Move Overhead value 150','isready')
  assert not any('ignored option Move Overhead' in l for l in e.log)
  record('Move Overhead remains available in Adaptive')
  e.sync('setoption name Adaptive value false','setoption name Move Overhead value 50')
  middle=chess.Board('r2q1rk1/pp3ppp/2pp1n2/2b1p1B1/2B1P3/2NP1N2/PPP2PPP/R2Q1RK1 w - - 4 8')
  for command in ['go wtime 1000 btime 1000 winc 2000 binc 2000','go wtime 1500 btime 1500 movestogo 1']:
   e.sync('ucinewgame','position fen '+middle.fen());ms,lines,best=search(middle,command,1700)
   record('clock/time-control deadline',command=command,ms=ms,best=best)
  e.sync('ucinewgame','position startpos');e.send('go infinite');e.until('info depth 3 ')
  end=time.monotonic()+.15
  while time.monotonic()<end:
   try:
    line=e.q.get(timeout=.03);assert line is not None;e.log.append(line);assert not line.startswith('bestmove ')
   except queue.Empty:pass
  start=time.monotonic();e.send('stop');e.until('bestmove ');assert time.monotonic()-start<1
  record('infinite analysis stays live and stop is responsive')
  single=chess.Board('7k/8/5K2/6Q1/8/8/8/8 b - - 0 1')
  assert single.is_valid() and single.legal_moves.count()==1
  e.sync('ucinewgame','position fen '+single.fen());ms,lines,best=search(single,'go movetime 60000',700)
  record('forced move returned immediately',ms=ms,best=best)
  selected={};varied=0
  for moves in ['e2e4','d2d4']:
   b=chess.Board();b.push_uci(moves);choices=[]
   for i in range(8):
    e.sync('ucinewgame','position startpos moves '+moves)
    ms,lines,best=search(b,'go movetime 700',1050);choices.append(best)
    if any('opening near-equal loss ' in l for l in lines):varied+=1
   selected[moves]=choices
  record('opening choices remain legal and can vary only through guarded selection',choices=selected,varied=varied)
  # Fixed-depth analysis is deliberately deterministic and has no opening randomness.
  e.sync('ucinewgame','position startpos');_,lines,_=search(chess.Board(),'go depth 4',5000)
  assert not any('opening near-equal' in l for l in lines)
  record('fixed-depth analysis does not use opening randomisation')
  e.sync('setoption name Style value Fire','setoption name UCI_Elo value 1200','setoption name UCI_LimitStrength value true','position startpos')
  _,lines,_=search(chess.Board(),'go movetime 500',800)
  assert not any(' multipv ' in l or 'opening near-equal' in l for l in lines)
  record('Elo bot keeps its original search/handicap path')
 finally:
  e.close();Path(a.log).write_text('\n'.join(e.log)+'\n',encoding='utf-8')
 Path(a.out).write_text(json.dumps({'status':'PASS','checks':results},indent=2),encoding='utf-8')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('engine');p.add_argument('--out',required=True);p.add_argument('--log',required=True);main(p.parse_args())
