/* Virgin 0.8.5. Classic dedicated Worker; UCI commands and output are strings. */
"use strict";
let engine, session = 0, output = 0, pollTimer, dead = false;
const queued = [];
function error(message) { self.postMessage({ type: "error", message: String(message) }); }
function dispose() {
  if (dead) return;
  dead = true;
  clearInterval(pollTimer);
  if (session) { engine._virgin_destroy(session); session = 0; }
  if (output) { engine._free(output); output = 0; }
  if (engine && engine.PThread) engine.PThread.terminateAllThreads();
  self.postMessage({ type: "closed" });
  self.close();
}
function send(command) {
  if (typeof command !== "string" || !command.trim() || /[\r\n\0]/.test(command)) {
    error("Expected one nonempty UCI line without newline or NUL"); return;
  }
  const bytes = engine.lengthBytesUTF8(command) + 1;
  if (bytes > 65537) { error("UCI command exceeds 65536 UTF-8 bytes"); return; }
  const pointer = engine._malloc(bytes);
  if (!pointer) { error("Cannot allocate UCI input"); return; }
  try {
    engine.stringToUTF8(command, pointer, bytes);
    if (engine._virgin_send(session, pointer) !== 1) error("Engine rejected UCI command");
  } finally { engine._free(pointer); }
}
function poll() {
  for (let count = 0; count < 256 && !dead; ++count) {
    const result = engine._virgin_read(session, output, 65537, 0);
    if (result === 1) self.postMessage(engine.UTF8ToString(output));
    else if (result === -2) break;
    else if (result === 0) { dispose(); break; }
    else { error("Engine output failure: " + result); dispose(); break; }
  }
}
self.onmessage = ({ data }) => {
  if (dead) return;
  if (data && typeof data === "object" && data.type === "dispose") {
    if (session) dispose(); else { dead = true; self.close(); }
    return;
  }
  const command = typeof data === "string" ? data : data && data.type === "command" ? data.command : null;
  if (typeof command !== "string") { error("Send a UCI string or {type:'command',command:'...'}"); return; }
  if (!session) {
    if (queued.length >= 256) error("Too many commands before engine initialization");
    else queued.push(command);
  } else send(command);
};
(async () => {
  try {
    if (!self.crossOriginIsolated || typeof SharedArrayBuffer === "undefined")
      throw new Error("Serve via HTTPS (or localhost) with COOP: same-origin and COEP: require-corp");
    const base = new URL("./", self.location.href);
    const { default: createVirgin } = await import(new URL("virgin.js", base).href);
    engine = await createVirgin({
      locateFile: name => new URL(name, base).href,
      print: text => self.postMessage({ type: "log", message: text }),
      printErr: text => self.postMessage({ type: "log", message: text }),
      onAbort: reason => error("WebAssembly aborted: " + reason)
    });
    if (dead) { engine.PThread.terminateAllThreads(); return; }
    session = engine._virgin_create();
    if (!session) throw new Error("Cannot create engine session");
    output = engine._malloc(65537);
    if (!output) throw new Error("Cannot allocate engine output buffer");
    pollTimer = setInterval(poll, 10);
    self.postMessage({ type: "ready", engine: "Virgin 0.8.5" });
    for (const command of queued.splice(0)) send(command);
  } catch (failure) {
    error(failure && failure.message || failure);
    dispose();
  }
})();
