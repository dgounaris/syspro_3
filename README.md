System programming: Assignment 3

This project contains an implementation of a multithreaded, network-based dropbox mimic, with a server and an arbitrary number of clients.

Server:
The server has the responsibility to be updated and inform new clients about active clients in the network. It keeps all the active clients in a list, which it shares with every requesting client through a network call.

Client:
The client consists of a master thread and an arbitrary amount of worker threads (currently only 1 worker thread is supported).
The responsibility of the master thread is to inform requesting clients about its own files and their information.
The responsibility of the worker threads is to request the aforementioned information from master sockets of other clients, guided by events published in a circular buffer of constant size.
Several semaphores and mutexes are used to ensure synchronous access to the buffer and the client lists.

To run the server you need to run:
`make dropbox_server`
followed by
`dropbox_server -p [ipport]`

To run the client you need to run:
`make dropbox_client`
followed by
`dropbox_client -p [ipport] -sip [serverip] -sp [serverport] -w [number of worker threads] -b [size of circular buffer] -d [private directory]

Designed and implemented by D. Gounaris (sdi1500032).
