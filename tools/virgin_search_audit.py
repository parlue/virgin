import argparse, chess, chess.engine, chess.pgn, io, json, pathlib, time

PGNS=[
'''1. e4 e5 2. Nf3 Nc6 3. Bb5 Nf6 4. O-O Nxe4 5. d4 Nd6 6. Bxc6 dxc6 7. dxe5 Nf5
8. Qe2 Nd4 9. Nxd4 Qxd4 10. h3 Bf5 11. Rd1 Qe4 12. Be3 Qxe5 13. g4 Bd7
14. Nc3 h5 15. f4 Qe7 16. g5 O-O-O 17. Qf3 Qe6 18. Rd3 Qxh3 19. Qxh3 Bxh3
20. Rxd8+ Kxd8 21. Bd4 Bf5 22. Rd1 Kc8 23. Re1 c5 24. Be5 Bxc2 25. Nd5 c6
26. Ne3 Bd3 27. Kg2 h4 28. Kh3 b5 29. b3 c4 30. bxc4 bxc4 31. Rc1 Ba3
32. Rxc4 Bxc4 33. Nxc4 Bf8 34. Bc3 Kd7 35. Ne5+ Ke6 36. Nxc6 Kf5 37. Be5 Bc5
38. Bxg7 Rc8 39. Nd4+ Kxf4 40. Kxh4 Bxd4 41. Bxd4 Rc2 42. Kh3 Rxa2 43. Bg7 Kxg5
44. Be5 a5 45. Kg3 Kf5 46. Bd4 a4 47. Kf3 Rd2 48. Ke3 Rxd4 49. Kxd4 a3
50. Kc3 Ke5 51. Kb3 f5 52. Kxa3 f4 53. Kb3 f3 54. Kc4 f2 55. Kd3 f1=Q+
56. Kc3 Ke4 57. Kb2 Kd3 58. Kb3 Qb1+ 59. Ka3 Kc4 60. Ka4 Qa2# 0-1''',
'''1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Bd6 5. O-O b5 6. Bb3 Nf6 7. d4 Qe7
8. dxe5 Bxe5 9. Nxe5 Nxe5 10. Nc3 Bb7 11. Bg5 O-O 12. Kh1 b4 13. Ne2 Bxe4
14. Ng3 d5 15. f3 Bg6 16. f4 Ned7 17. f5 c6 18. Qd2 Rfe8 19. a3 bxa3
20. Rxa3 Ne4 21. Nxe4 Qxe4 22. fxg6 Qxg6 23. Be3 Nf6 24. Ba4 Ne4 25. Qd4 Red8
26. Qb6 Rd6 27. Qc7 Rf8 28. Bd4 Qg4 29. Rg1 Qe2 30. h3 Rg6 31. Rd3 a5
32. Kh2 h6 33. Qxa5 Rc8 34. Qa6 Rf8 35. Qa7 Re8 36. Qb7 Ree6 37. b4 Rg5
38. Qb8+ Kh7 39. Qf4 Reg6 40. Qf3 Ng3 41. Be3 Rf5 42. Qxe2 Nxe2 43. Re1 Nf4
44. Rd2 Re6 45. Rf2 Rff6 46. Bd4 Rxe1 47. Bxf6 Re4 48. Bc3 c5 49. Bd7 Rc4
50. b5 Rxc3 51. Rxf4 Kg8 52. b6 Rxc2 53. Rb4 cxb4 54. b7 Rf2 55. b8=Q+ Kh7
56. Qxb4 Rf6 57. Qe7 h5 58. Bb5 d4 59. Bd3+ Kh6 60. h4 Rf5 61. Bxf5 g6
62. Qf8+ Kh7 63. Qxf7+ Kh8 64. Bxg6 d3 65. Qf8# 1-0'''
]

def positions():
    result=[]
    for number,(pgn,plies) in enumerate(zip(PGNS,[[24,28,34,62,94,98],[7,31,33,43,99,105]]),1):
        game=chess.pgn.read_game(io.StringIO(pgn));assert not game.errors
        board=game.board()
        for ply,move in enumerate(game.mainline_moves()):
            if ply in plies:
                result.append((f'loss{number}-ply{ply}',board.copy(stack=True),move))
            board.push(move)
        assert board.is_checkmate()
    return result

def run(args):
    results=[]
    with chess.engine.SimpleEngine.popen_uci(args.engine,timeout=20) as engine:
        engine.configure({'Threads':args.threads,'UCI_LimitStrength':False,'Hash':32})
        for style in args.styles:
            engine.configure({'Style':style})
            for label,board,played in positions():
                if args.cases and label not in args.cases:continue
                engine.configure({'Clear Hash':None})
                limit=chess.engine.Limit(nodes=args.nodes) if args.nodes else chess.engine.Limit(time=args.ms/1000)
                started=time.monotonic()
                info=engine.analyse(board,limit,game=object())
                pv=info.get('pv',[])
                # Every reported PV move is independently replayed by python-chess.
                check=board.copy()
                for move in pv:
                    assert move in check.legal_moves,(label,pv)
                    check.push(move)
                record=dict(case=label,fen=board.fen(),played=board.san(played),style=style,
                            engine=engine.id,threads=args.threads,limit_nodes=args.nodes,limit_ms=args.ms,
                            elapsed_ms=round((time.monotonic()-started)*1000),depth=info.get('depth'),
                            nodes=info.get('nodes'),nps=info.get('nps'),score=info.get('score').pov(board.turn).score(mate_score=31000),
                            best=board.san(pv[0]) if pv else None,pv=[m.uci() for m in pv])
                results.append(record)
                print(label,style,'played',record['played'],'new',record['best'],'cp',record['score'],
                      'depth',record['depth'],'nodes',record['nodes'],flush=True)
    pathlib.Path(args.out).write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('engine');parser.add_argument('--out',required=True)
    parser.add_argument('--threads',type=int,default=1);parser.add_argument('--nodes',type=int,default=0)
    parser.add_argument('--ms',type=int,default=1000);parser.add_argument('--styles',nargs='+',default=['Ausgeglichen'])
    parser.add_argument('--cases',nargs='+')
    run(parser.parse_args())
