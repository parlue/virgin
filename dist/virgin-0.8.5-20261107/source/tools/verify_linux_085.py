from pathlib import Path
import subprocess,queue,threading,time,json,struct,shutil
r=Path(__file__).resolve().parents[1];d=r/'dist/virgin-0.8.5-20261107/linux';reports=r/'reports'
zig=r/'build/linux-toolchain/zig-x86_64-linux-0.15.2/zig'
shutil.copytree(r/'data',d/'data',dirs_exist_ok=True)
for f in (d/'data').glob('*'):
 if f.is_file():f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
elf=[]
for arch in ['x86_64','armv6','armv7']:
 f=d/('virgin-0.8.5-linux-'+arch)
 b=f.read_bytes();assert b[:4]==b'\x7fELF'
 machine=struct.unpack_from('<H',b,18)[0];assert machine==(62 if arch=='x86_64' else 40)
 details=subprocess.check_output(['readelf','-h','-l','-d','-A',str(f)],text=True)
 assert 'INTERP' not in details and '(NEEDED)' not in details
 if arch=='armv6':assert 'Tag_CPU_arch: v6' in details and 'hard-float ABI' in details
 if arch=='armv7':assert 'Tag_CPU_arch: v7' in details and 'hard-float ABI' in details
 (reports/('linux-'+arch+'-elf.txt')).write_text(details)
 elf.append({'architecture':arch,'bytes':len(b),'machine':machine,'static':True,'elf_check':'PASS','hardware_run':arch=='x86_64'})
(reports/'linux-elf-085.json').write_text(json.dumps(elf,indent=2))
exe=d/'virgin-0.8.5-linux-x86_64'
result=subprocess.run([str(exe),'--selftest',str(d/'data')],capture_output=True,text=True)
(reports/'linux-x86_64-selftest.log').write_text(result.stdout+result.stderr)
assert result.returncode==0,result.stdout[-300:]
p=subprocess.Popen([str(exe)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1,cwd=d)
q=queue.Queue();lines=[]
def reader():
 for line in p.stdout:q.put(line.strip())
threading.Thread(target=reader,daemon=True).start()
def send(s):p.stdin.write(s+'\n');p.stdin.flush()
def until(prefix):
 end=time.monotonic()+15;found=[]
 while time.monotonic()<end:
  try:line=q.get(timeout=.2)
  except queue.Empty:continue
  lines.append(line);found.append(line)
  if line.startswith(prefix):return found
 raise AssertionError(('timeout',prefix,lines[-10:]))
checks=[]
try:
 send('uci');hello=until('uciok');assert 'id name Virgin 0.8.5' in hello;checks.append('UCI 0.8.5 identity')
 send('setoption name Threads value 1');send('setoption name LivePerformance value true');send('ucinewgame');send('position startpos moves e2e4');send('go depth 4');until('bestmove ')
 send('performance');perf=until('info string performance white ')[-1];assert 'white_samples 1 ' in perf and 'pending 0 ' in perf and perf.endswith('status ready');checks.append('Confirmed-move performance query')
 send('ucinewgame');send('performance');assert 'white >2800 black >2800' in until('info string performance white ')[-1];checks.append('New-game reset')
 send('position startpos');send('perft 4');assert any('197281' in x for x in until('Nodes:'));checks.append('Perft(4) 197281')
 send('go infinite');until('info depth 3 ');start=time.monotonic();send('stop');until('bestmove ');assert time.monotonic()-start<3;checks.append('Infinite-search stop responsive')
finally:
 send('quit');p.wait(timeout=5)
(reports/'linux-uci-085.json').write_text(json.dumps({'status':'PASS','checks':checks},indent=2))
(reports/'linux-uci-085.txt').write_text('\n'.join(lines))
print('PASS Linux x64 selftest +',len(checks),'UCI checks; all three static ELF architectures checked')
