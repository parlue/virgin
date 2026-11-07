// Browser integration smoke test. Node 22+; uses built-in fetch and WebSocket.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawn} = require('node:child_process');
const ROOT = path.resolve(__dirname, '..');
const DIST = process.argv[2] || path.join(ROOT,'dist/virgin-web-0.8.5-20261107');
const python = process.env.VIRGIN_TEST_PYTHON || (process.platform === 'win32' ? 'python' : 'python3');
const chrome = process.env.VIRGIN_TEST_CHROME || path.join(process.env.PROGRAMFILES || 'C:/Program Files', 'Google/Chrome/Application/chrome.exe');
const sleep = ms => new Promise(resolve=>setTimeout(resolve,ms));
async function retry(fn, ms=20000) {
 const until=Date.now()+ms;let last;
 while(Date.now()<until){try{return await fn();}catch(error){last=error;await sleep(200);}}
 throw last;
}
async function connect(url) {
 const socket=new WebSocket(url);let id=0;const calls=new Map();
 await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
 socket.onmessage=({data})=>{const message=JSON.parse(data);const pending=calls.get(message.id);if(pending){calls.delete(message.id);message.error?pending.reject(new Error(JSON.stringify(message.error))):pending.resolve(message.result);}};
 return {socket,send(method,params={}){return new Promise((resolve,reject)=>{const next=++id;calls.set(next,{resolve,reject});socket.send(JSON.stringify({id:next,method,params}));});}};
}
const browserTest = async () => {
 const checks=[], lines=[], errors=[];
 const assert=(condition,label)=>{if(!condition)throw new Error(label);checks.push(label);};
 assert(crossOriginIsolated && typeof SharedArrayBuffer!=='undefined','Cross-origin isolation and shared memory');
 const worker=new Worker('./engine-worker.js');
 let ready=false,closed=false;
 worker.onmessage=({data})=>{
  if(typeof data==='string')lines.push(data);
  else if(data.type==='ready')ready=true;
  else if(data.type==='closed')closed=true;
  else if(data.type==='error')errors.push(data.message);
 };
 worker.onerror=event=>errors.push(event.message);
 const wait=async(predicate,label,timeout=20000)=>{
  const end=Date.now()+timeout;
  while(!predicate()){if(errors.length)throw new Error(errors.join('\n'));if(Date.now()>end)throw new Error('Timeout: '+label+'\n'+lines.slice(-12).join('\n'));await new Promise(r=>setTimeout(r,10));}
 };
 const send=line=>worker.postMessage(line);
 const until=async(command,regex,timeout)=>{const at=lines.length;send(command);await wait(()=>lines.slice(at).some(x=>regex.test(x)),command,timeout);return lines.slice(at);};
 try {
  await wait(()=>ready,'runtime ready',60000);
  await until('uci',/^uciok$/);
  assert(lines.includes('id name Virgin 0.8.5')&&lines.includes('id author Dirk D. Sommerfeld'),'Correct engine identity');
  assert(lines.some(x=>x.startsWith('option name Adaptive type check')),'Adaptive UCI option');
  assert(lines.some(x=>x.includes('var Fortress var Stone var Normal var Fire var Thunder')),'All five English styles');
  assert(lines.some(x=>/^option name Threads .* max 4$/.test(x)),'Browser thread limit advertised');
  assert(lines.some(x=>/^option name Hash .* max 256$/.test(x)),'Browser hash limit advertised');
  await until('isready',/^readyok$/);
  assert(lines.some(x=>x==='option name LivePerformance type check default false'),'LivePerformance option advertised');
  send('setoption name LivePerformance value true');
  send('ucinewgame');send('position startpos moves e2e4');
  await until('go movetime 1500',/^bestmove /);
  const perf=await until('performance',/^info string performance white /);
  assert(perf.some(x=>/white_samples 1\b/.test(x)&&/pending 0\b/.test(x)&&/status ready$/.test(x)),'Live performance measures confirmed move and query returns ready');
  send('setoption name LivePerformance value false');send('ucinewgame');
  send('position startpos');
  const perft=await until('perft 4',/Nodes:\s+197281\b/);
  assert(perft.some(x=>/Nodes:\s+197281\b/.test(x)),'Start-position perft(4) = 197281');
  send('setoption name Threads value 1');
  send('position startpos moves e2e4 e7e5 g1f3');
  let search=await until('go movetime 250',/^bestmove [a-h][1-8][a-h][1-8][qrbn]?$/);
  assert(search.some(x=>x.startsWith('info depth ')),'Single-thread timed search emits analysis and bestmove');
  send('setoption name Threads value 4');
  await until('isready',/^readyok$/);
  search=await until('go movetime 250',/^bestmove [a-h][1-8][a-h][1-8][qrbn]?$/);
  assert(search.some(x=>x.startsWith('info depth ')),'Four-thread timed search completes');
  const at=lines.length;
  send('go infinite');
  await wait(()=>lines.slice(at).some(x=>x.startsWith('info depth ')),'infinite search begins');
  await new Promise(resolve=>setTimeout(resolve,100));
  const start=performance.now();
  await until('stop',/^bestmove /,5000);
  assert(performance.now()-start<5000,'Stop interrupts a running analysis without killing the worker');
  send('setoption name Adaptive value true');
  send('ucinewgame');send('position startpos');
  search=await until('go movetime 500',/^bestmove [a-h][1-8][a-h][1-8][qrbn]?$/);
  assert(search.some(x=>x.startsWith('info string adaptive ')),'Adaptive move selection runs in WebAssembly');
  send('setoption name Adaptive value false');
  send('setoption name Style value Fire');send('setoption name UCI_Elo value 1200');send('setoption name UCI_LimitStrength value true');
  await until('isready',/^readyok$/);
  assert(lines.some(x=>x.includes('strength limited to 1200 elo')),'Elo bot configuration accepted');
  await until('go movetime 250',/^bestmove [a-h][1-8][a-h][1-8][qrbn]?$/);
  send('setoption name UCI_LimitStrength value false');send('setoption name Style value Normal');
  send('position fen 7k/1P6/8/8/8/8/8/7K w - - 0 1');
  search=await until('go depth 4',/^bestmove /);
  assert(search.some(x=>/^bestmove b7b8[qrbn]$/.test(x)),'Promotion suffix survives the worker interface');
  send('position startpos');const closeAt=lines.length;send('go infinite');
  await wait(()=>lines.slice(closeAt).some(x=>x.startsWith('info depth ')),'search before dispose');
  worker.postMessage({type:'dispose'});
  await wait(()=>closed,'dispose during search',5000);
  assert(closed && errors.length===0,'Dispose stops active search and closes the session cleanly');
  const clean=new Worker('./engine-worker.js');let cleanReady=false,cleanClosed=false;
  clean.onmessage=({data})=>{if(data.type==='ready'){cleanReady=true;clean.postMessage('quit');}if(data.type==='closed')cleanClosed=true;if(data.type==='error')errors.push(data.message);};
  clean.onerror=event=>errors.push(event.message);
  await wait(()=>cleanReady&&cleanClosed,'second fresh worker / quit',60000);
  clean.terminate();assert(true,'Fresh worker can initialize again and quit normally');
  return {status:'PASS',checks,lines:lines.slice(-45),errors};
 } finally {worker.terminate();}
};
(async()=>{
 const profile=fs.mkdtempSync(path.join(os.tmpdir(),'virgin-wasm-browser-'));
 const server=spawn(python,[path.join(DIST,'serve.py'),'--port','8768','--directory',DIST],{windowsHide:true,stdio:'ignore'});
 const browser=spawn(chrome,['--headless=new','--no-first-run','--no-default-browser-check','--disable-extensions','--remote-debugging-port=9338','--user-data-dir='+profile,'about:blank'],{windowsHide:true,stdio:'ignore'});
 let control,page;
 try {
  await retry(async()=>{const r=await fetch('http://127.0.0.1:8768/ENGINE-LICENSE.txt');if(!r.ok)throw new Error('Server not ready');});
  const info=await retry(async()=>{const r=await fetch('http://127.0.0.1:9338/json/version');return r.json();});
  control=await connect(info.webSocketDebuggerUrl);
  const target=await control.send('Target.createTarget',{url:'http://127.0.0.1:8768/ENGINE-LICENSE.txt'});
  const tabs=await(await fetch('http://127.0.0.1:9338/json/list')).json();
  page=await connect(tabs.find(t=>t.id===target.targetId).webSocketDebuggerUrl);
  await page.send('Runtime.enable');
  await retry(async()=>{const r=await page.send('Runtime.evaluate',{expression:'document.readyState',returnByValue:true});if(r.result.value!=='complete')throw new Error('Document loading');});
  const result=await page.send('Runtime.evaluate',{expression:'('+browserTest.toString()+')()',awaitPromise:true,returnByValue:true,timeout:180000});
  if(result.exceptionDetails)throw new Error(JSON.stringify(result.exceptionDetails));
  const report={browser:info.Browser,...result.result.value};
  fs.writeFileSync(path.join(ROOT,'reports/web-browser-test-0.8.5-20261107.json'),JSON.stringify(report,null,2));
  console.log(JSON.stringify(report,null,2));
  if(report.status!=='PASS')process.exitCode=1;
 } catch(error){console.error(error.stack||error);process.exitCode=1;}
 finally {
  if(page)page.socket.close();
  if(control){await Promise.race([control.send('Browser.close').catch(()=>{}),sleep(1000)]);control.socket.close();}
  browser.kill();server.kill();
 }
})();
