# wyn-api

A complete REST API server in **93 lines** of Wyn. 71KB binary.

Demonstrates: HTTP server, spawn concurrency, HashMap, string interpolation, pattern matching.

## Run

```bash
wyn run
# or build a binary:
wyn build    # → 71KB binary
./main
```

## Endpoints

```
GET    /api/health        - health check
GET    /api/items         - list all items
GET    /api/items/:id     - get one item
POST   /api/items         - create item (body = name)
DELETE /api/items/:id     - delete item
```

## Try it

```bash
# Health check
curl localhost:8080/api/health
# → {"status":"ok","items":0}

# Create items
curl -X POST -d 'Buy milk' localhost:8080/api/items
# → {"id":1,"name":"Buy milk","done":false}

curl -X POST -d 'Ship Wyn 1.8' localhost:8080/api/items
# → {"id":2,"name":"Ship Wyn 1.8","done":false}

# List all
curl localhost:8080/api/items
# → [{"id":1,...},{"id":2,...}]

# Delete
curl -X DELETE localhost:8080/api/items/1
# → {"deleted":1}
```

## How it works

- **HTTP server**: `Http.serve(port)` + `Http.accept(server)` for request handling
- **Concurrency**: Each request handled in a separate green thread via `spawn handle(...)`
- **Storage**: In-memory `HashMap` - no external dependencies
- **JSON**: Built with string interpolation - `"{"id":${id},"name":"${name}"}"`
