## Requirements:
Boost.Asio 1.81 and higher

## How to build
Once cloned, run "make" in the root repository folder, executable will be created in ./bin.

## How to run
Run ./node with arguments -server and -port. You can set multiple servers to connect: once connection with i-th server is lost, the node will attempt to connect server with number i+1 in the list.
Example:
```
./node -server 127.0.0.1:9000 -port 9002 -server 127.0.0.1:9001 -server 127.0.0.1:9002
```
the node will try to connect 127.0.0.1:9000, if it doesn't work - 127.0.0.1:9001, if it doesn't work as well, it will connect server that is running within this node

## A bit of architecture
This is a TCP-based solution. Every node in the network can be be both server and client. The role of server is broadcasting of all transactions to all clients connected to it, making sure that
