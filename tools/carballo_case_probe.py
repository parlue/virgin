import argparse, json, pathlib, queue, subprocess, threading, time

MOVES = 'e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5c6 d7c6 h2h3 c8e6 e1g1 d8d6 b2b3 e8c8 c1b2 f7f6 d1e2 c6c5 a2a4 g8e7 b1a3 e7g6 a3c4 d6d7 e2e3 d7e8 d2d3 g6f4 h3h4 e8h5 g2g3 e6h3 g1h2 h3f1 a1f1 f4e6 e3e2 g7g5 e2d1 h8g8 c4e3 f8d6 c2c3 d6f8 g3g4 h5f7 h4g5 h7h5 g5f6 f7f6 e3d5 d8d5 e4d5 h5g4 f3g5 g8h8 h2g2 e6f4 g2g3 f4e2 g3g2 e2f4 g2g3 f4e2 g3g2'.split()

def probe(engine, threads, ms, checkpoints):
    p = subprocess.Popen([engine], cwd=pathlib.Path(engine).parent, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, text=True, encoding='utf-8', errors='replace')
    lines = queue.Queue()
    def pump():
        for line in p.stdout:
            lines.put(line.rstrip())
    threading.Thread(target=pump, daemon=True).start()
    def send(s):
        p.stdin.write(s + '\n'); p.stdin.flush()
    def read(marker):
        result = []
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            line = lines.get(timeout=max(.01, deadline-time.monotonic()))
            result.append(line)
            if line.startswith(marker):
                return result
        raise TimeoutError(marker)
    results = []
    try:
        send('uci'); handshake = read('uciok')
        version = next(s for s in handshake if s.startswith('id name'))
        for style in ['Ausgeglichen', 'Angriffslustig']:
            send(f'setoption name Threads value {threads}')
            send('setoption name UCI_LimitStrength value false')
            send(f'setoption name Style value {style}')
            for ply in checkpoints:
                send('ucinewgame')
                send('setoption name Clear Hash')
                send('position startpos moves ' + ' '.join(MOVES[:ply]))
                send('d'); board = read('Checkers:')
                if any('illegal move' in s for s in board):
                    raise ValueError(board)
                fen = next(s[5:] for s in board if s.startswith('Fen: '))
                send('eval'); ev = read('info string terms:')
                send('isready'); read('readyok')
                start = time.monotonic()
                send(f'go movetime {ms}')
                search = read('bestmove ')
                record = dict(version=version, style=style, threads=threads, ms=ms, ply=ply,
                              fen=fen, static=ev, elapsed_ms=round((time.monotonic()-start)*1000),
                              info=[s for s in search if s.startswith('info depth')][-1:],
                              bestmove=search[-1])
                results.append(record)
                print(style, ply//2+1, record['bestmove'], record['info'], flush=True)
        send('quit'); p.wait(timeout=5)
        return results
    finally:
        if p.poll() is None:
            p.kill(); p.wait()

if __name__ == '__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('engine'); ap.add_argument('--threads',type=int,default=1)
    ap.add_argument('--ms',type=int,default=1000); ap.add_argument('--out',required=True)
    ap.add_argument('--plies',type=int,nargs='+',default=[16,24,28,30,32,46,50,56])
    args=ap.parse_args()
    result=probe(str(pathlib.Path(args.engine).resolve()),args.threads,args.ms,args.plies)
    pathlib.Path(args.out).write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
