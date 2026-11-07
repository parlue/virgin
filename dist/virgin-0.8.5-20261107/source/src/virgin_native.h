#pragma once
#ifdef _WIN32
#define VIRGIN_API __declspec(dllexport)
#else
#define VIRGIN_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
// One live session per process. A second create returns null until destroy.
// All commands/answers are UTF-8 UCI lines without trailing newline.
// Read continuously to consume search info. A handle must outlive all calls.
VIRGIN_API void* virgin_create(void);
// 1 accepted, 0 closed, -1 invalid (empty/multiline/>65536 bytes).
VIRGIN_API int virgin_send(void* session, const char* command);
// 1 line read; 0 session ended; -1 invalid; -2 timeout; -3 buffer too small.
// Capacity includes NUL. A too-small buffer leaves the line queued.
VIRGIN_API int virgin_read(void* session, char* output, int capacity, int timeout_ms);
// Queues stop/quit, joins all engine workers, releases resources. Do not call
// concurrently with any other operation on this handle, including read.
VIRGIN_API void virgin_destroy(void* session);
#ifdef __cplusplus
}
#endif
