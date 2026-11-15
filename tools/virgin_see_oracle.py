import argparse, chess, importlib.util, json, pathlib, queue, subprocess, threading

VALUES={chess.PAWN:100,chess.KNIGHT:395,chess.BISHOP:405,chess.ROOK:595,chess.QUEEN:1225,chess.KING:20000}

def gain(board,move):
    result=VALUES[chess.PAWN] if board.is_en_passant(move) else VALUES.get(board.piece_type_at(move.to_square),0)
    return result+(VALUES[move.promotion]-VALUES[chess.PAWN] if move.promotion else 0)

def recapture_value(board,target):
    best=0
    for move in list(board.generate_legal_captures(to_mask=chess.BB_SQUARES[target])):
        value=gain(board,move);board.push(move)
        value-=recapture_value(board,target);board.pop()
        best=max(best,value)
    return best

def oracle(board,move):
    value=gain(board,move);board.push(move)
    value-=recapture_value(board,move.to_square);board.pop()
    return value

def cases():
    spec=importlib.util.spec_from_file_location('audit',pathlib.Path(__file__).with_name('virgin_search_audit.py'))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    samples=[]
    for pgn in module.PGNS:
        import io, chess.pgn
        game=chess.pgn.read_game(io.StringIO(pgn));board=game.board()
        for ply,move in enumerate(game.mainline_moves()):
            if ply%3==0:
                legal=list(board.legal_moves)
                moves=[m for m in legal if board.is_capture(m) or m.promotion]
                moves += [m for m in legal if not board.is_capture(m) and not m.promotion][:2]
                samples.extend((board.copy(stack=False),m) for m in moves)
            board.push(move)
    for fen,text in [('7k/7p/5n2/7Q/8/8/1B6/6KR w - - 0 1','h5h7'),
                     ('7k/8/8/3pP3/8/8/8/K7 w - d6 0 1','e5d6'),
                     ('k4r2/6P1/8/8/8/8/8/K7 w - - 0 1','g7g8q')]:
        board=chess.Board(fen);move=chess.Move.from_uci(text)
        assert move in board.legal_moves
        samples.append((board,move))
    return samples

def main(args):
    p=subprocess.Popen([args.engine],cwd=pathlib.Path(args.engine).parent,stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE,text=True,encoding='utf-8',errors='replace')
    lines=queue.Queue()
    def pump():
        for line in p.stdout:lines.put(line.strip())
    threading.Thread(target=pump,daemon=True).start()
    def send(text):p.stdin.write(text+'\n');p.stdin.flush()
    def until(prefix):
        while True:
            line=lines.get(timeout=10)
            if line.startswith(prefix):return line
    results=[]
    try:
        send('uci');until('uciok');send('setoption name Threads value 1')
        send('setoption name Style value Ausgeglichen');send('isready');until('readyok')
        for board,move in cases():
            expected=oracle(board,move)
            send('position fen '+board.fen());send('see '+move.uci())
            line=until('info string see ')
            assert 'invalid' not in line,(board.fen(),move,line)
            actual=int(line.split()[4])
            results.append(dict(fen=board.fen(),move=move.uci(),expected=expected,actual=actual))
        send('quit');p.wait(timeout=5)
    finally:
        if p.poll() is None:p.kill();p.wait()
    failures=[r for r in results if r['expected']!=r['actual']]
    pathlib.Path(args.out).write_text(json.dumps(dict(cases=len(results),differences=failures,results=results),indent=2),encoding='utf-8')
    print('Independent legal recapture oracle:',len(results),'cases,',len(failures),'differences')
    for row in failures[:8]:print(row)
    if failures:raise SystemExit(1)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('engine');p.add_argument('--out',required=True)
    main(p.parse_args())
