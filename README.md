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

Once node is running, you can enter commands to control it. There are three commands available:
```
new_position [instrument name] [quantity] - pushes position on the given instrument
e.g.
new_position BTCUSDT.BN 10
```
```
show_positions - will list all positions available on the current node
```
```
generate [name] [num_positions] [period, ms] - will generate %num_positions% different positions, new position is created every %period% ms
```

## A bit of architecture
This is a TCP-based solution. Every node in the network can be be both server and client at the same time. The role of server is broadcasting of all transactions to all clients connected to it, making sure that their local repositories are up to date.

*How do we ensure correctness?*
To make sure that every update is received and processed, the following is implemented: 1) whole thing uses TCP, so we are sure that message will be received, we are not sure that it will be processed correctly, so 2) message acknowledgement is also implemented on the level of application; 3) both client and server track sequence number of updates, in case of missing message it will be requested directly. 

*How do we ensure order preservation?*
We ensure that messages in local repositories are sorted by sequence numbers.

*How do we ensure resilience?*
In case any server node is dropped all client nodes reconnect to the next server in the list of their parameters. They keep doing that until available server is found.

## Some problems of this version

A bit too many requests are generated: load should be optimized.

History requests do not work well when lots of messages comming in (can work if several history messages are merged into one big message, but this is not implemented since I tried to keep it as simple as possible).

Unfortunately, data races are possible here, the lack of time played out.

As the consequence of above, some parts of the program have been intentionally made slower than they could be (not acceptable for a production code).






