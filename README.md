# NOT MAINTAINED

Note that this is a project that was developed internally at The Mode Group
and has now been released under the Apache-2.0 license (see LICENSE and
NOTICE).  The Mode Group is not actively maintaining this project and is not
currently accepting pull requests, bug reports, or changes.  Users of this
project are welcome to fork it under the allowable terms of its license and
continue the project at their own discretion.

# C-proxy
A TCP proxy with TCP-fast-open (TFO) feature in C 

It listens on an address-port pair and proxies them to a backend address-port pair.

Implementation: different master threads are used to manage different end-to-end connections, and different slave threads (2 slaves for each master) are used to manage upstream and downstream connection for one end-to-end connection seperately.

By default, TFO is turned on. It can be turn off by commenting out `#define TFO_ENABLED 1` of `src/tcp_proxy.c`.

# Build and Run on Local Machine
Build and run:

	gcc -pthread -o src/tcp_proxy src/tcp_proxy.c
	src/tcp_proxy <listenIp> <listenPort> <backendIp> <backendPort>

You need to specify listen ip, listen port, backend ip, backend port. For example:

	src/tcp_proxy 0.0.0.0 80 10.1.1.2 5000

would proxy all connections from 0.0.0.0:80 to 10.1.1.2:5000.

# Run in Container

	sudo docker container build -t <name> .
	sudo docker run <name> <listenIp> <listenPort> <backendIp> <backendPort>

# Test
Some sample client/server programs for manual testing (you need to properly configure the file names and port numbers):

1. non_tfo_1: TFO disabled on end-hosts; server is the sender. 

2. non_tfo_2: TFO disabled on end-hosts; client is the sender.

3. tfo_1: TFO enabled on end-hosts; server is the sender.

4. tfo_2: TFO enabled on end-hosts; client is the sender.

# Note

The host machine should have TFO enabled, i.e., you should

	echo "3" > /proc/sys/net/ipv4/tcp_fastopen
