"""Run the Android x86_64 UCI executable in the isolated test emulator."""
import subprocess,threading,queue,time,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
DEVICE='emulator-5580'
def adb(*args):return subprocess.check_output([str(ADB),'-s',DEVICE,*args],text=True).strip()
checks=[];log=[]
exe=ROOT/'build/android-oex-x86_64/virgin'
adb('push',str(exe),'/data/local/tmp/virgin-082');adb('shell','chmod','755','/data/local/tmp/virgin-082')
p=subprocess.Popen([str(ADB),'-s',DEVICE,'shell','/data/local/tmp/virgin-082'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
q=queue.Queue()
def reader():
 for line in p.stdout:q.put(line.rstrip())
 q.put(None)
threading.Thread(target=reader,daemon=True).start()
def send(s):log.append('> '+s);p.stdin.write(s+'\n');p.stdin.flush()
def until(prefix):
 out=[];end=time.monotonic()+20
 while time.monotonic()<end:
  line=q.get(timeout=max(.01,end-time.monotonic()));assert line is not None
  log.append(line);out.append(line)
  if line.startswith(prefix):return out
 raise AssertionError(prefix)
def ready(*commands):
 for command in commands:send(command)
 send('isready');return until('readyok')
try:
 send('uci');lines=until('uciok');assert 'id name Virgin 0.8.2' in lines
 checks.append('Android x86_64 UCI identity 0.8.2')
 ready('setoption name Threads value 1','position startpos');send('perft 4');lines=until('Nodes:')
 assert any('Nodes: 197281' in line for line in lines);checks.append('Android perft(4) = 197281')
 send('go movetime 300');lines=until('bestmove ');assert lines[-1]!='bestmove 0000';checks.append('Fixed-time search completes')
 ready('setoption name Adaptive value true','ucinewgame','position startpos moves e2e4')
 send('go movetime 500');lines=until('bestmove ')
 assert any('info string adaptive ' in line for line in lines);checks.append('Adaptive uses native Android search and selection')
 ready('setoption name Move Overhead value 100','position startpos');start=time.monotonic();send('go wtime 0 btime 0');until('bestmove ')
 assert time.monotonic()-start<2;checks.append('Exhausted clock stays bounded')
 send('go infinite');until('info depth 2 ');send('stop');until('bestmove ');checks.append('Infinite analysis / stop')
 send('quit');p.wait(timeout=10)
finally:
 if p.poll() is None:p.kill();p.wait()
 (ROOT/'reports/android-0.8.2-uci.txt').write_text('\n'.join(log),encoding='utf-8')
(ROOT/'reports/android-0.8.2-uci.json').write_text(json.dumps({'status':'PASS','device':DEVICE,'checks':checks},indent=2),encoding='utf-8')
print('\n'.join('PASS '+c for c in checks))
