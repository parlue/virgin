import argparse,pathlib,json,time,sys,queue
sys.path.insert(0,str(pathlib.Path(r'C:\Users\DirkSommerfeld\Documents\projects\virgin\tools')))
from virgin_adaptive_uci_test import Engine,verify
import chess
def main(a):
    e=Engine(a.engine);results=[]
    try:
        e.send('uci');e.until('uciok')
        for adaptive in [False,True]:
            e.sync('setoption name Adaptive value '+str(adaptive).lower(),'ucinewgame','position startpos moves e2e4')
            board=chess.Board();board.push_uci('e2e4')
            start=time.monotonic();lines=e.search('go wtime 7380000 btime 7200000 winc 0 binc 0');ms=round((time.monotonic()-start)*1000)
            best=chess.Move.from_uci(lines[-1].split()[1]);assert best in board.legal_moves
            assert ms<4500,(adaptive,ms,lines[-3:])
            results.append(dict(case='BearChess 1.e4 two-hour clock',adaptive=adaptive,ms=ms,best=best.uci()))
        e.sync('setoption name Adaptive value false','ucinewgame','position startpos moves e2e4')
        start=time.monotonic();lines=e.search('go');ms=round((time.monotonic()-start)*1000)
        assert ms<5500 and chess.Move.from_uci(lines[-1].split()[1]) in board.legal_moves
        results.append(dict(case='bare go is bounded',ms=ms,best=lines[-1]))
        e.sync('ucinewgame','position startpos moves e2e4');start=time.monotonic();lines=e.search('go wtime 0 btime 0');ms=round((time.monotonic()-start)*1000)
        assert ms<500 and chess.Move.from_uci(lines[-1].split()[1]) in board.legal_moves
        results.append(dict(case='zero clock is bounded',ms=ms,best=lines[-1]))
        single=None
        for q in chess.SQUARES:
            if q in [chess.F6,chess.H8]:continue
            b=chess.Board(None);b.set_piece_at(chess.F6,chess.Piece(chess.KING,chess.WHITE));b.set_piece_at(chess.H8,chess.Piece(chess.KING,chess.BLACK));b.set_piece_at(q,chess.Piece(chess.QUEEN,chess.WHITE));b.turn=chess.BLACK
            if b.is_valid() and b.legal_moves.count()==1:single=b;break
        e.sync('ucinewgame','position fen '+single.fen());start=time.monotonic();lines=e.search('go movetime 60000');ms=round((time.monotonic()-start)*1000)
        assert ms<500 and chess.Move.from_uci(lines[-1].split()[1]) in single.legal_moves
        results.append(dict(case='only legal move finishes without spending 60 seconds',ms=ms,best=lines[-1]))
        mate='6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1'
        e.sync('ucinewgame','position fen '+mate);e.send('go infinite');e.until('info depth 1 ')
        end=time.monotonic()+.25;spontaneous=[]
        while time.monotonic()<end:
            try:
                line=e.q.get(timeout=.03);assert line is not None;e.log.append(line);spontaneous.append(line)
            except queue.Empty:pass
        assert not any(l.startswith('bestmove ') for l in spontaneous)
        e.send('stop');lines=e.until('bestmove ');assert chess.Move.from_uci(lines[-1].split()[1]) in chess.Board(mate).legal_moves
        results.append(dict(case='infinite analysis waits for stop even with known mate',best=lines[-1]))
    finally:e.close();pathlib.Path(a.log).write_text('\n'.join(e.log)+'\n',encoding='utf-8')
    pathlib.Path(a.out).write_text(json.dumps(results,indent=2),encoding='utf-8')
    for item in results:print('PASS',item)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('engine');p.add_argument('--out',required=True);p.add_argument('--log',required=True);main(p.parse_args())
