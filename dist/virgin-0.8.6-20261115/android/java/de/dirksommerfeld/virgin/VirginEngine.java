package de.dirksommerfeld.virgin;

/** One session per process. Run readLine() on a background reader thread.
 * Calls are serialized so close() cannot free a handle during read/send.
 * Close on a background thread: it waits for the native search to finish.
 */
public final class VirginEngine implements AutoCloseable {
    static { System.loadLibrary("virgin"); }
    private long handle;
    public VirginEngine() {
        handle = nativeCreate();
        if (handle == 0) throw new IllegalStateException("Virgin is already open or could not start");
    }
    public synchronized void send(String command) {
        ensureOpen();
        if (command == null || command.indexOf('\0') >= 0) throw new IllegalArgumentException("Invalid UCI command");
        int result = nativeSend(handle, command);
        if (result == -1) throw new IllegalArgumentException("Expected one nonempty UCI line, max 65536 bytes");
        if (result != 1) throw new IllegalStateException("Virgin session ended");
    }
    /** Returns one UCI line, or null on timeout/after quit. Use 0..100 ms.
     * Large timeouts delay concurrent send/close because calls serialize.
     */
    public synchronized String readLine(int timeoutMs) {
        ensureOpen();
        if (timeoutMs < 0 || timeoutMs > 100) throw new IllegalArgumentException("Timeout must be 0..100 ms");
        return nativeRead(handle, timeoutMs);
    }
    public synchronized boolean isOpen() { return handle != 0; }
    @Override public synchronized void close() {
        if (handle != 0) { nativeClose(handle); handle = 0; }
    }
    private void ensureOpen() {
        if (handle == 0) throw new IllegalStateException("Virgin is closed");
    }
    private static native long nativeCreate();
    private static native int nativeSend(long handle, String command);
    private static native String nativeRead(long handle, int timeoutMs);
    private static native void nativeClose(long handle);
}
