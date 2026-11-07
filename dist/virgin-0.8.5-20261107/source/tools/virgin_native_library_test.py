import ctypes as C,sys,time,json,pathlib,chess,subprocess
lib=C.CDLL(sys.argv[1]);lib.virgin_create.restype=C.c_void_p
lib.virgin_send.argtypes=[C.c_void_p,C.c_char_p];lib.virgin_send.restype=C.c_int
lib.virgin_read.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.c_int];lib.virgin_read.restype=C.c_int
lib.virgin_destroy.argtypes=[C.c_void_p]
checks=[];log=[]
def send(h,s):assert lib.virgin_send(h,s.encode())==1;log.append('> '+s)
def until(h,prefix):
    lines=[];end=time.monotonic()+15;b=C.create_string_buffer(65536)
    while time.monotonic()<end:
        rc=lib.virgin_read(h,b,len(b),100)
        if rc==-2:continue
        assert rc==1,(rc,lines[-5:]);line=b.value.decode();log.append('< '+line);lines.append(line)
        if line.startswith(prefix):return lines
    raise AssertionError(('timeout',prefix,lines[-5:]))
def sync(h,*commands):
    for s in commands:send(h,s)
    send(h,'isready');return until(h,'readyok')
h=lib.virgin_create();assert h
try:
    assert not lib.virgin_create();checks.append('A second concurrent session is rejected')
    b=C.create_string_buffer(1);assert lib.virgin_read(h,b,1,0)==-2
    send(h,'uci');assert lib.virgin_read(h,b,1,5000)==-3
    lines=until(h,'uciok');assert 'id author Dirk D. Sommerfeld' in lines and any(line.startswith('id name Virgin ') for line in lines)
    checks.append('UCI identity, timeout and undersized-buffer retention')
    for invalid in [b'',b'uci\nquit',b'uci\rquit',b'x'*65537]:assert lib.virgin_send(h,invalid)==-1
    checks.append('Malformed and oversized input is rejected')
    sync(h,'setoption name Threads value 1','position startpos moves e2e4')
    send(h,'go depth 6');lines=until(h,'bestmove ');best=chess.Move.from_uci(lines[-1].split()[1]);board=chess.Board();board.push_uci('e2e4');assert best in board.legal_moves
    info=[s for s in lines if s.startswith('info depth 6 ')][-1]
    # Same algorithm/position/threads in the standalone executable must agree.
    # Wait interactively: EOF intentionally stops the stream front end.
    p=subprocess.Popen([sys.argv[2]],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
    p.stdin.write('setoption name Threads value 1\nposition startpos moves e2e4\ngo depth 6\n');p.stdin.flush();exe=[]
    while True:
        s=p.stdout.readline().strip();exe.append(s)
        if s.startswith('bestmove '):break
    p.stdin.write('quit\n');p.stdin.flush();p.wait(timeout=5)
    baseline=[s for s in exe if s.startswith('info depth 6 ')][-1]
    def result(s):return (s.split('score cp ')[1].split()[0],s.split('nodes ')[1].split()[0],s.split(' pv ')[1])
    assert result(info)==result(baseline),(info,baseline)
    checks.append('Depth-6 score, node count and PV match the standalone engine exactly')
    sync(h,'setoption name Adaptive value true','ucinewgame','position startpos moves e2e4')
    send(h,'go movetime 300');lines=until(h,'bestmove ')
    assert any('multipv 5 ' in s for s in lines) and any('info string adaptive ' in s for s in lines)
    assert chess.Move.from_uci(lines[-1].split()[1]) in board.legal_moves
    checks.append('Adaptive emits five candidates, selection and a legal bestmove')
    sync(h,'setoption name Adaptive value false','position startpos')
    send(h,'go infinite');until(h,'info depth 2 ');send(h,'isready');until(h,'readyok');send(h,'stop');until(h,'bestmove ')
    checks.append('isready and stop remain responsive during infinite search')
    send(h,'go infinite');until(h,'info depth 2 ');t=time.monotonic();lib.virgin_destroy(h);h=None;assert time.monotonic()-t<3
    checks.append('Destroy stops and joins an active search')
finally:
    if h:lib.virgin_destroy(h)
for i in range(12):
    h=lib.virgin_create();assert h
    try:
        send(h,'uci');lines=until(h,'uciok');assert 'option name Adaptive type check default false' in lines
        sync(h,'setoption name Threads value 1','position startpos');send(h,'go depth 2');until(h,'bestmove ')
        send(h,'quit');b=C.create_string_buffer(65536)
        end=time.monotonic()+5
        while True:
            rc=lib.virgin_read(h,b,len(b),100)
            if rc==0:break
            assert rc in [1,-2] and time.monotonic()<end
        assert lib.virgin_send(h,b'isready')==0
    finally:lib.virgin_destroy(h)
checks.append('12 create/search/quit/destroy cycles reset state and finish cleanly')
pathlib.Path('virgin-library-test.json').write_text(json.dumps(dict(passed=checks),indent=2),encoding='utf8')
pathlib.Path('virgin-library-test.txt').write_text('\n'.join(log),encoding='utf8')
print('\n'.join('PASS '+s for s in checks))
