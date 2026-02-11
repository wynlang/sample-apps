# Web Applications

HTTP web services and servers built with Wyn.

## Web Server

A simple HTTP web server demonstrating:
- TCP socket server with `Net::listen()`
- Async request handling
- HTTP response generation
- HTML content serving

### Usage

```bash
cd web-server
../../wyn/wyn main.wyn
./main.wyn.out
```

Then visit: `http://localhost:8080`

### Features

- ✅ TCP socket server (Net::listen)
- ✅ Async request handling with `async fn` and `await`
- ✅ HTTP response generation with proper headers
- ✅ HTML content serving
- ✅ String manipulation for protocol handling

### Code Example

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

async fn serve_request(socket: int) -> int {
    var request = Net::recv(socket);
    if request.len() > 0 {
        var response = build_response();
        Net::send(socket, response);
    }
    Net::close(socket);
    return 1;
}

fn main() -> int {
    var server = Net::listen(8080);
    var handled = await serve_request(server);
    Net::close(server);
    return 0;
}
```

### Output

```
=== Wyn Web Server ===
Port: 8080

Server running!
Visit: http://localhost:8080

Waiting for connection...
Received request
Served: 1
```

## More Web Examples

Future additions:
- REST API server
- WebSocket server
- Static file server
- Template engine
