import collections, importlib.util, pathlib, subprocess
path=pathlib.Path(__file__).with_name('virgin_case2_probe.py')
spec=importlib.util.spec_from_file_location('game',path)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
exe=m.ROOT/'dist/virgin-0.3.0-20261031/virgin-0.3.0.exe'
commands=[]
for n in range(64):
    commands+=['position startpos'+(' moves '+' '.join(m.MOVES[:n]) if n else ''),'d']
commands+=['quit']
result=subprocess.run([str(exe)],input='\n'.join(commands)+'\n',text=True,stdout=subprocess.PIPE,check=True)
keys=[s[5:] for s in result.stdout.splitlines() if s.startswith('Key: ')]
assert len(keys)==64
counts=collections.Counter(keys)
print('Final key occurrences:',[i for i,k in enumerate(keys) if k==keys[-1]])
print('Maximum occurrences of any position:',max(counts.values()))
for n in [26,28,30,38,40,42,44,46,62,63]:
    fens=[s[5:] for s in result.stdout.splitlines() if s.startswith('Fen: ')]
    print(n,fens[n])
commands=['setoption name Style value Angriffslustig']
for n in [26,28,30,38,40,42,44,46,62,63]:
    commands+=['position startpos moves '+' '.join(m.MOVES[:n]),'eval']
commands+=['quit']
result=subprocess.run([str(exe)],input='\n'.join(commands)+'\n',text=True,stdout=subprocess.PIPE,check=True)
print(result.stdout)
