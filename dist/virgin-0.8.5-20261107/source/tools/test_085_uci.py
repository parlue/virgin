from pathlib import Path
import sys,tempfile,json,chess,chess.engine,time
p=Path(__file__).resolve().parents[1];sys.path.insert(0,str(p/'tools'))
from virgin_adaptive_uci_test import Engine
exe=Path(sys.argv[1]) if len(sys.argv)>1 else p/'build/final/virgin.exe';checks=[]
def record(s):checks.append(s);print('PASS',s,flush=True)
e=Engine(str(exe))
try:
 e.send('uci');hello=e.until('uciok');assert 'id name Virgin 0.8.5' in hello;assert any('option name MultiPV type spin default 1 min 1 max 5' in l for l in hello);record('0.8.5 identity and MultiPV option')
 for threads in [1,4]:
  e.sync('setoption name Threads value '+str(threads),'setoption name MultiPV value 2','ucinewgame','position startpos')
  lines=e.search('go searchmoves e2e4 d2d4 depth 4 nodes 200000')
  assert lines[-1].split()[1] in ['e2e4','d2d4'];seen={}
  for l in lines:
   if l.startswith('info depth ') and ' multipv ' in l and ' pv ' in l:
    b=chess.Board();pv=l.split(' pv ',1)[1].split();assert pv[0] in ['e2e4','d2d4']
    for u in pv:m=chess.Move.from_uci(u);assert m in b.legal_moves;b.push(m)
    if 'info depth 4 ' in l:seen[int(l.split(' multipv ')[1].split()[0])]=pv[0]
  assert len(seen)==2 and len(set(seen.values()))==2,(threads,lines[-15:]);record('Restricted MultiPV legal and distinct with '+str(threads)+' threads; limits after searchmoves parsed')
 e.sync('setoption name Threads value 1','setoption name MultiPV value 1','ucinewgame','position startpos')
 lines=e.search('go depth 5 searchmoves e2e4');assert lines[-1]=='bestmove e2e4';assert any(l.startswith('info depth 5 ') for l in lines);record('Single candidate reaches requested depth')
 e.sync('ucinewgame','position startpos');lines=e.search('go searchmoves e2e4 e2e4 a1a8 depth 3');assert lines[-1]=='bestmove e2e4';record('Duplicate/illegal candidate cannot escape allowed list')
 e.sync('ucinewgame','position startpos');lines=e.search('go searchmoves a1a8 depth 3');assert lines[-1]=='bestmove 0000';record('Empty legal candidate list returns no move')
 e.sync('setoption name MultiPV value 99','ucinewgame','position startpos');lines=e.search('go depth 2');ranks={int(l.split(' multipv ')[1].split()[0]) for l in lines if l.startswith('info depth 2 ') and ' multipv ' in l};assert ranks=={1,2,3,4,5};record('MultiPV clamps to five')
 e.sync('setoption name MultiPV value 1','setoption name Adaptive value true','ucinewgame','position startpos');lines=e.search('go nodes 80000 searchmoves e2e4 d2d4');assert lines[-1].split()[1] in ['e2e4','d2d4'];record('Adaptive respects restricted root set')
 e.sync('setoption name Adaptive value false','ucinewgame','position startpos');e.send('go infinite searchmoves e2e4 d2d4');e.until('info depth 3 ');t=time.monotonic();e.send('stop');lines=e.until('bestmove ');assert time.monotonic()-t<3;assert lines[-1].split()[1] in ['e2e4','d2d4'];record('Restricted infinite search stops responsively')
finally:e.close()
(p/'reports/uci-0.8.5-new.json').write_text(json.dumps({'status':'PASS','checks':checks},indent=2),encoding='utf-8')