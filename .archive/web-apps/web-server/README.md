# Wyn Web Server

A simple HTTP web server demonstrating TCP networking and async/await in Wyn v1.4.0.

## Features

- ✅ TCP socket server with `Net::listen()`
- ✅ Async request handling with `async fn` and `await`
- ✅ HTTP response generation
- ✅ String manipulation and concatenation
- ✅ HTML content serving

## Usage

```bash
cd sample-apps/showcase/web-server
../../../wyn/wyn main.wyn
./main.wyn.out
```

Then visit: `http://localhost:8080`

## Code Highlights

**HTTP Response Building:**
```wyn
fn build_response() -> string {
    var body = build_html();
    var response = "HTTP/1.1 200 OK\r\n";
    response = response.concat("Content-Type: text/html\r\n");
    response = response.concat("Content-Length: ");
    response = response.concat(body.len().to_string());
    response = response.concat("\r\n\r\n");
    response = response.concat(body);
    return response;
}
```

**Async Request Handling:**
```wyn
async fn serve_request(socket: int) -> int {
    var request = Net::recv(socket);
    if request.len() > 0 {
        var response = build_response();
        Net::send(socket, response);
    }
    Net::close(socket);
    return 1;
}

var handled = await serve_request(server);
```

## Features Demonstrated

1. **TCP Networking**
   - `Net::listen(port)` - Create server socket
   - `Net::recv(socket)` - Receive data
   - `Net::send(socket, data)` - Send data
   - `Net::close(socket)` - Close connection

2. **Async/Await**
   - `async fn` - Asynchronous functions
   - `await` - Non-blocking operations

3. **String Processing**
   - `.concat()` - String concatenation
   - `.len()` - String length
   - `.to_string()` - Type conversion

4. **HTTP Protocol**
   - Response headers
   - Content-Length calculation
   - HTML content serving

## Output

```
=== Wyn Web Server ===
Port: 8080

Server running!
Visit: http://localhost:8080

Waiting for connection...
Received request
Served: 1
...
Server stopped
```

## Notes

This is a simplified demonstration server. A production server would:
- Use proper `accept()` for connections
- Handle multiple concurrent clients
- Parse HTTP requests properly
- Support routing and methods
- Include error handling

The current implementation demonstrates the core networking and async capabilities of Wyn v1.4.0.
