from pathlib import Path
import sys,json,time,chess
sys.path.insert(0,str(Path(__file__).parent))
from virgin_adaptive_uci_test import Engine,weights
root=Path(__file__).resolve().parents[1];(root/'reports').mkdir(exist_ok=True);exe=sys.argv[1];e=Engine(exe);checks=[]
report_tag=sys.argv[2] if len(sys.argv)>2 else 'performance-0.8.5'
def done(label):checks.append(label);print('PASS',label,flush=True)
def snapshot():
    e.send('performance');lines=e.until('info string performance white ')
    tokens=lines[-1].split()[3:];return dict(zip(tokens[::2],tokens[1::2]))
def search(go='go depth 4'):
    lines=e.search(go);return lines[-1].split()[1]
try:
    e.send('uci');hello=e.until('uciok');assert any(line.startswith('id name Virgin 0.8.5') for line in hello)
    assert 'option name LivePerformance type check default false' in hello
    assert snapshot()['active']=='false';done('Option and disabled default')
    e.sync('setoption name Threads value 1','setoption name LivePerformance value true','ucinewgame','position startpos moves e2e4')
    p=snapshot();assert p['pending']=='1' and p['white_samples']=='0'
    best=search();p=snapshot();assert p['white_samples']=='1' and p['pending']=='0',p
    done('Position queued, full-strength player measurement on go, query never starts search')
    e.sync('position startpos moves e2e4');search();assert snapshot()['white_samples']=='1'
    done('Repeated position/search does not count a move twice')
    e.sync('position startpos moves e2e4 '+best)
    board=chess.Board();board.push_uci('e2e4');board.push_uci(best)
    reply=next(iter(board.legal_moves)).uci();moves='e2e4 '+best+' '+reply
    e.sync('position startpos moves '+moves);search();p=snapshot()
    assert (p['white_samples'],p['black_samples'],p['pending'])==('2','1','0'),p
    done('Both colours tracked independently; selected engine move committed only after actual position')
    e.sync('position startpos moves d2d4');p=snapshot();assert p['white_samples']=='0'
    search();assert snapshot()['white_samples']=='1';done('Takeback/branch resets invalid history')
    e.sync('setoption name Adaptive value true','setoption name LivePerformance value false')
    assert snapshot()['active']=='true';e.sync('setoption name Adaptive value false')
    assert snapshot()['active']=='false';done('Adaptive forces evaluation on and restores optional normal setting')
    e.sync('setoption name LivePerformance value true','ucinewgame','position startpos')
    assert snapshot()['white']=='>2800' and snapshot()['white_samples']=='0'
    done('New game restores >2800 and clears evidence')
    e.send('go infinite');e.until('info depth 2 ')
    t=time.monotonic();p=snapshot();assert time.monotonic()-t<1
    e.send('stop');e.until('bestmove ');done('Read-only query remains responsive during search')
    for i in range(5):
        e.sync('ucinewgame','position startpos moves e2e4')
        e.send('go infinite');e.send('stop');e.until('bestmove ',timeout=5)
        assert int(snapshot()['white_samples'])<=1
    done('Cancellation survives serial performance/main searches and rapid stop')
    e.sync('ucinewgame','position startpos moves e2e4');search('go movetime 1')
    assert snapshot()['white_samples']=='0';search();assert snapshot()['white_samples']=='1'
    done('Tiny time budgets defer measurement honestly, then process pending work once')
    e.sync('ucinewgame','position startpos');e.send('performance prepare');e.until('info string performance prepared depth ')
    assert snapshot()['white_samples']=='0';e.sync('position startpos moves e2e4');search()
    assert snapshot()['white_samples']=='1';done('Optional preparation analyses candidates without inventing a played move')
    e.sync('ucinewgame','position startpos moves e2e4');black=search()
    board=chess.Board();board.push_uci('e2e4');board.push_uci(black);legal=list(board.legal_moves)
    assert len(legal)>1
    predicted=legal[0].uci();actual=legal[1].uci()
    e.sync('position startpos moves e2e4 '+black+' '+predicted)
    e.send('go ponder');e.until('info depth 2 ');p=snapshot();assert p['white_samples']=='1',p
    e.send('stop');e.until('bestmove ')
    e.sync('position startpos moves e2e4 '+black+' '+actual);search();p=snapshot()
    assert p['white_samples']=='2' and p['black_samples']=='1',p
    done('Speculative ponder move never counted; miss preserves confirmed history')
    e.sync('ucinewgame','position startpos moves e2e4','setoption name UCI_Elo value 700','setoption name UCI_LimitStrength value true')
    before=weights(e.sync('weights','eval'));search('go depth 5');after=weights(e.sync('weights','eval'))
    assert before==after;assert snapshot()['white_samples']=='1'
    done('Performance measurement restores all handicap/knowledge settings')
    e.sync('ucinewgame','position startpos moves e2e4','setoption name LivePerformance value false')
    search();p=snapshot();assert p['active']=='false' and p['white_samples']=='0'
    done('Disabled mode performs no measurements')
finally:
    e.close();(root/'reports'/(report_tag+'-transcript.txt')).write_text('\n'.join(e.log),encoding='utf8')
(root/'reports'/(report_tag+'.json')).write_text(json.dumps({'status':'PASS','checks':checks},indent=2),encoding='utf8')
print('ALL PASS',len(checks))
