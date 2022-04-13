# SOL-Progetto

Multi-threaded file storage server implemented in C

## Tests

Three tests are available. Change dir using `cd server` and then execute one of the following commands:

1.  `make test1`  **_valgrind test_**: server executed using valgrind to demonstrate the server's correct behaviour and memory usage. Only one client active at a time which sends a new request every 200ms. Server threadpool size: **1**

2.  `make test2`  **_eviction test_**: Only one client active at a time which sends a new request every 50ms. Requests are specifically made to test the server's eviction policies. Server threadpool size: **4**

3.  `make test3`  **_stress test_**: Ten clients run simultaneously for 30s, making new requests with no delay. Demonstrates the server's concurrency handling. Server threadpool size: **8**

Some stats are automatically calculated and printed at the end of each test
## Instructions
To test the app by yourself execute the following commands:
```
cd server
make bin/server bin/clients
```
_Server_ usage: `bin/server <config file> <p>`  
The third parameter can be anything, its presence enables server's `stdout` printing

_Client_ usage: `bin/client <options>`
The client parses cli arguments to determine which requests it has to send
