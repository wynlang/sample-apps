# Networking Applications

TCP/IP networking examples demonstrating Wyn's networking capabilities.

## HTTP Client

A TCP-based HTTP client demonstrating:
- TCP socket connections with `Net::connect()`
- Async/await for I/O operations
- HTTP protocol implementation
- String parsing and manipulation

### Usage

```bash
cd http-client
../../wyn/wyn main.wyn
./main.wyn.out
```

### Features

- ✅ TCP networking (Net::connect, send, recv, close)
- ✅ Async/await for non-blocking I/O
- ✅ HTTP request building
- ✅ Response parsing
- ✅ String manipulation

### Code Example

```wyn
async fn fetch_url(host: str, port: int) -> int {
    var socket = Net::connect(host, port);
    var request = "GET / HTTP/1.1\r\nHost: ";
    request = request.concat(host);
    request = request.concat("\r\n\r\n");
    
    Net::send(socket, request);
    var response = Net::recv(socket);
    Net::close(socket);
    
    return response.len();
}

var bytes = await fetch_url("example.com", 80);
```

## More Networking Examples

See also:
- `web-apps/web-server` - HTTP server implementation
- Future: WebSocket client, FTP client, etc.
