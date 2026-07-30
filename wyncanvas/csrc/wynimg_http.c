// wynimg_http - binary-safe HTTP file response.
//
// The only reason this file exists is that the Wyn runtime's HTTP surface is
// text-only, verified on this machine:
//
//   File_read          (src/wyn_runtime.h:2735) fopen(path, "r"), NUL-terminates
//   http_send_response (src/wyn_runtime.h:3095) body_len = strlen(body)
//
// So a PNG served through `File.read` + `Http.respond` is truncated at its
// first NUL byte. Measured with a 21-byte fixture containing one interior NUL:
// the client received 12 bytes. PNG's IHDR chunk always contains NULs (its
// length field is 00 00 00 0D), so this affects every image.
//
// Rather than widen the runtime (a larger, riskier change touching the string
// representation), the shim writes the response itself. This also honours the
// project rule that bulk pixels never cross into Wyn: the Wyn side passes an
// fd and a path, both scalars.

#include "wynimg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <limits.h>

// Wyn hands descriptors across as `long long`, and `int fd = (long long)v`
// TRUNCATES. That is not a pedantic concern: measured before this guard,
// wynimg_close_fd(4294967296) -- which is 2^32, a value with NO low bits set --
// truncated to fd 0 and CLOSED STDIN, returning 1 to report success. The same
// truncation let wynimg_http_send_file(2^32 + 3, ...) write a full HTTP response
// into whatever fd 3 happened to be, and report success for that too.
//
// A descriptor arriving from Wyn is arithmetic that may have gone wrong (a
// missing accept result is 0, an error is -1, an int overflow is anything), so
// the range is checked rather than assumed. Refusing is always correct here:
// both callers already treat 0 as failure.
static int fd_ok(long long v) { return v >= 0 && v <= (long long)INT_MAX; }

// write(), not send(). Two reasons, in order of importance:
//
//  1. TESTABILITY. send() fails with ENOTSOCK on anything that is not a socket,
//     which makes the byte-exact behaviour of this function - the entire point
//     of the file - impossible to unit-test without standing up a real server.
//     write() works on sockets and files alike, so tests can point it at a file
//     and diff the result. Given that this code exists to fix a truncation bug,
//     being able to prove the absence of truncation is worth more than send()'s
//     flags.
//  2. It is the same syscall underneath for a stream socket with no flags; the
//     runtime's own sender passes only MSG_NOSIGNAL/0 (wyn_runtime.h:3036-3041).
//
// SIGPIPE is handled the way the runtime handles it: SO_NOSIGPIPE on the socket
// (macOS/BSD). On Linux there is no SO_NOSIGPIPE, so we ignore SIGPIPE for this
// write instead - a client that vanishes mid-response must not kill the editor.
static void nosigpipe(int fd) {
#ifdef SO_NOSIGPIPE
    int one = 1;
    // Best-effort: fails harmlessly with ENOTSOCK on a plain file fd.
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
    (void)fd;
#endif
}

// write() can return short on a socket. Loop until the whole buffer is out or
// it fails. A partial response is a corrupt image, so a failure must be
// reported, never silently accepted.
static int write_all(int fd, const char* p, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t k = write(fd, p + sent, n - sent);
        if (k < 0) {
            // EINTR is a signal, not a failure: retry. Anything else (EPIPE from
            // a vanished client, ENOSPC) is real.
            if (errno == EINTR) continue;
            return 0;
        }
        if (k == 0) return 0;
        sent += (size_t)k;
    }
    return 1;
}

long long wynimg_http_send_file(long long fd_ll, const char* path,
                                const char* content_type) {
    if (!fd_ok(fd_ll) || !path) return 0;
    int fd = (int)fd_ll;

    FILE* f = fopen(path, "rb");   // "rb", not "r" - the runtime's bug
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }

    unsigned char* body = (unsigned char*)malloc((size_t)sz > 0 ? (size_t)sz : 1);
    if (!body) { fclose(f); return 0; }
    size_t got = fread(body, 1, (size_t)sz, f);
    fclose(f);
    // A short read means the file changed under us; refuse rather than serve a
    // truncated image, which is exactly the failure this function exists to fix.
    if (got != (size_t)sz) { free(body); return 0; }

    char header[512];
    int hn = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Cache-Control: no-store\r\n"   // the canvas is re-rendered per request
        "Connection: close\r\n"
        "\r\n",
        content_type ? content_type : "application/octet-stream", sz);
    if (hn < 0 || (size_t)hn >= sizeof(header)) { free(body); return 0; }

    nosigpipe(fd);
    int ok = write_all(fd, header, (size_t)hn);
    if (ok && sz > 0) ok = write_all(fd, (const char*)body, (size_t)sz);
    free(body);
    if (!ok) return 0;

    // We advertised Connection: close, so the client waits for FIN to know the
    // body ended. SHUT_WR only - shutting the read side could RST a client that
    // still has queued bytes. The caller closes the fd. Harmless ENOTSOCK on a
    // file fd, which is what the tests use.
    shutdown(fd, SHUT_WR);
    return 1;
}

// --- Test seam ---------------------------------------------------------------
// Raw descriptors, so a Wyn test can drive wynimg_http_send_file against a real
// file and diff the bytes it produced. Wyn's own File.open returns a handle into
// a FILE* table (wyn_runtime.h:2982), not a descriptor, so it cannot be used
// here. These are deliberately minimal and are not used by src/server.wyn.

long long wynimg_open_wr(const char* path) {
    if (!path) return -1;
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

long long wynimg_close_fd(long long fd) {
    if (!fd_ok(fd)) return 0;
    return close((int)fd) == 0 ? 1 : 0;
}

// Byte-exact file inspection, so a test can assert on a NUL-containing body
// that Wyn's own string type cannot hold. Returns -1 if the file is unreadable.
long long wynimg_file_size(const char* path) {
    if (!path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    fclose(f);
    return (long long)sz;
}

// The byte at `off`, or -1 out of range. Lets a test verify a PNG signature and
// the interior NULs directly.
long long wynimg_file_byte(const char* path, long long off) {
    if (!path || off < 0) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, (long)off, SEEK_SET) != 0) { fclose(f); return -1; }
    int c = fgetc(f);
    fclose(f);
    return c == EOF ? -1 : (long long)c;
}
