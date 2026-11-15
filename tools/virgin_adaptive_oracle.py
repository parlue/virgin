import argparse, copy, importlib.util, json, pathlib, random, subprocess
import chess, chess.pgn, io, sys

SOURCE=pathlib.Path(r'C:\Users\DirkSommerfeld\Documents\CynusTactics')
spec=importlib.util.spec_from_file_location('original_tal',SOURCE/'tal/tal_chooser.py')
original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
PARAMS=original.load_params(SOURCE/'app/src/main/assets/tal/tal_params.json')
START=chess.STARTING_FEN
MOVES=['e2e4','d2d4','g1f3','c2c4','b1c3']
class FixedRandom:
    def __init__(self,roll,pick):self.roll=roll;self.pick=pick
    def random(self):return self.roll
    def choice(self,pool):return pool[min(len(pool)-1,int(self.pick*len(pool)))]
def case(scores,ply=16,state=None,fen=START,pvs=None,roll=1.0,pick=0.0,name=''):
    if pvs is None:pvs=[[m] for m in MOVES[:len(scores)]]
    cands=[]
    for score,pv in zip(scores,pvs):
        cands.append(dict(move=pv[0],cp=score if isinstance(score,int) else None,
                          mate=score[1] if isinstance(score,tuple) else None,pv=pv))
    return dict(name=name,fen=fen,ply=ply,state=state or {},cands=cands,roll=roll,pick=pick)
def fixtures():
    yield case([70,40,0,-30,-60],ply=0,name='opening')
    yield case([350,300,290,280,270],name='hopeless player')
    yield case([('mate',5),90,60,30,0],name='near mate')
    yield case([70,40,0,-30,-60],name='equalisation')
    yield case([220,180,150,100,0],name='help')
    yield case([220,0,-30,-60,-100],name='no safe help step')
    yield case([-230,-250,-260,-270,-280],name='sticky hold reached')
    yield case([50,0,-100,-180,-250],state={'hold':True},name='sticky hold later')
    yield case([20,-20,-60,-100,-140],ply=14,state={'band':2,'acc':[False]*6},name='adaptation ladder')
    yield case([20,-20,-60,-100,-140],ply=14,state={'band':2,'acc':[True]*6},name='accurate player protected')
    yield case([20,-20,-60,-100,-140],state={'adjust_until':20},name='adaptation grace')
    yield case([20,-20,-60,-100,-140],state={'tactic_until':20},name='tactic continuation')
    yield case([20,-20,-60,-100,-140],state={'expected':100,'acc':[True]*12},name='accuracy window')
    yield case([70,40,0,-30,-60],roll=0.0,pick=0.9,name='random equivalent')
    yield case([200,160,130],fen='7k/8/8/3q4/4Q3/8/8/6K1 w - - 0 1',
               pvs=[['e4d5','h8h7'],['e4e5'],['e4d4']],name='protect material combination')
    yield case([60,0,-20],fen='4r2k/8/8/8/4Q3/8/8/6K1 w - - 0 1',
               pvs=[['e4d4','h8h7'],['e4e5','e8e5'],['e4c4']],name='reject queen donation')
    yield case([('mate',-2),('mate',-4),('mate',-6)],name='losing mate distances')
    sys.path.insert(0,str(pathlib.Path(r'C:\Users\DirkSommerfeld\Documents\projects\virgin\tools')))
    from virgin_search_audit import PGNS
    material_rng=random.Random(13301)
    for game_id,pgn in enumerate(PGNS):
        game=chess.pgn.read_game(io.StringIO(pgn));board=game.board();line=list(game.mainline_moves())
        for ply,played in enumerate(line):
            if ply%4==0:
                alternatives=list(board.legal_moves);material_rng.shuffle(alternatives)
                alternatives=[played]+[m for m in alternatives if m!=played][:4]
                pvs=[]
                for rank,move in enumerate(alternatives):
                    b=board.copy();b.push(move);pv=[move.uci()]
                    if rank==0:
                        for m in line[ply+1:ply+6]:pv.append(m.uci());b.push(m)
                    else:
                        while len(pv)<6 and not b.is_game_over():
                            legal=list(b.legal_moves);captures=[m for m in legal if b.is_capture(m)]
                            m=material_rng.choice(captures or legal);pv.append(m.uci());b.push(m)
                    pvs.append(pv)
                yield case([200,160,120,80,40][:len(pvs)],ply=ply,fen=board.fen(),pvs=pvs,name=f'material-game-{game_id}-ply-{ply}')
            board.push(played)
    rng=random.Random(79831)
    for i in range(600):
        scores=sorted([rng.randint(-400,400) for _ in range(rng.randint(1,5))],reverse=True)
        state=dict(hold=rng.random()<.1,helped=rng.randrange(6),tactic_until=rng.choice([-1,14,20]),
                   adjusts=rng.randrange(4),last_adjust=rng.choice([-999,4,14]),adjust_until=rng.choice([-1,14,20]),
                   band=rng.randrange(5),expected=rng.choice([None,-250,-100,0,50,220]),
                   acc=[rng.random()<.7 for _ in range(rng.randrange(13))])
        yield case(scores,ply=rng.choice([0,6,8,14,16,20,40]),state=state,
                   roll=rng.random(),pick=rng.random(),name=f'random-{i}')
def main(args):
    cases=list(fixtures());blocks=[];expected=[];modes={}
    for item in cases:
        state=original.TalState()
        for key,value in item['state'].items():setattr(state,key,copy.deepcopy(value))
        acc=list(state.acc)
        header=[item['ply'],int(state.hold),state.helped,state.tactic_until,state.adjusts,state.last_adjust,
                state.adjust_until,state.band,int(state.expected is not None),state.expected or 0,len(acc),
                item['roll'],item['pick'],len(item['cands'])]+[int(v) for v in acc]
        blocks.extend([item['fen'],' '.join(map(str,header))])
        for c in item['cands']:
            score=c['cp'] if c['mate'] is None else (31000-2*abs(c['mate'])+(1 if c['mate']>0 else 0))*(1 if c['mate']>0 else -1)
            blocks.append(' '.join(map(str,[c['move'],score,len(c['pv']),*c['pv']])))
        move,mode,info=original.choose(chess.Board(item['fen']),item['cands'],item['ply'],state,PARAMS,FixedRandom(item['roll'],item['pick']))
        chosen=next(i for i,c in enumerate(item['cands']) if c['move']==move)
        c=item['cands'][chosen];after=-(c['cp'] if c['mate'] is None else (100000-abs(c['mate']))*(1 if c['mate']>0 else -1))
        accuracy=info['accuracy'];percent=-1 if accuracy is None else int(accuracy*100+.5)
        expected.append(' '.join(map(str,[chosen,mode,info['E'],after,percent,int(state.hold),state.helped,state.tactic_until,
                    state.adjusts,state.last_adjust,state.adjust_until,state.band,state.expected,len(state.acc),*[int(v) for v in state.acc]])))
        modes[mode]=modes.get(mode,0)+1
    result=subprocess.run([args.engine,'--adaptive-probe'],input='\n'.join(blocks)+'\n',text=True,capture_output=True,timeout=40)
    assert result.returncode==0,(result.returncode,result.stderr,result.stdout[-1000:])
    actual=result.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected))
    differences=[dict(case=cases[i],expected=e,actual=a) for i,(e,a) in enumerate(zip(expected,actual)) if e!=a]
    report=dict(cases=len(cases),differences=differences,modes=modes,source=str(SOURCE/'tal/tal_chooser.py'))
    pathlib.Path(args.out).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Original TalChooser parity:',len(cases),'cases,',len(differences),'differences; modes',modes)
    for difference in differences[:4]:print(difference)
    assert not differences
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('engine');p.add_argument('--out',required=True);main(p.parse_args())

