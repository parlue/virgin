import argparse, importlib.util, json, pathlib

ROOT=pathlib.Path(r'C:\Users\DirkSommerfeld\Documents\projects\virgin')
spec=importlib.util.spec_from_file_location('case_probe',ROOT/'tools/carballo_case_probe.py')
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
MOVES='e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4 e5d4 e1g1 d4c3 b1c3 d7d6 c1g5 e8g8 c3d5 c8g4 g5f6 g7f6 d1d3 g8h8 a1e1 c6e5 f3e5 f6e5 d5c3 d8h4 c3d1 f7f5 d1e3 f5f4 e3f5 h4g5 b2b4 c5b4 e1b1 a7a5 a2a3 f8f5 h2h4 g5g6 a3b4 f5h5 b4a5 h5h4 f2f3 g4h3 d3d2 h3c8 a5a6 b7b6 d2d5 a8a7 f1c1 g6g7 d5c6 c8d7 c6d5 d7c8 d5c6 c8d7 c6d5'.split()
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('engine');ap.add_argument('--threads',type=int,default=1)
    ap.add_argument('--ms',type=int,default=1000);ap.add_argument('--out',required=True)
    ap.add_argument('--plies',type=int,nargs='+',default=[26,28,30,34,38,40,46,62])
    args=ap.parse_args(); module.MOVES=MOVES
    result=module.probe(str(pathlib.Path(args.engine).resolve()),args.threads,args.ms,args.plies)
    pathlib.Path(args.out).write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
