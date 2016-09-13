Implementation Details:

As send and recv are both blocking, a serial implementation requires us to convert send and recv to the nonblocking versions. This makes the serial version more inefficient as the CPU might be busy looping when the message actually arrives. Therefore, we use select();


Threading is implementedas follows: We have an array fdarray (of fixed length MAX_CONN_HIGH_WATERMARK) storing file descriptors. Our main thread handles connections, dumping file descriptors into fdarray whenever a connection is received. Then we have a number of threads (implemented via posix threads) working on forwarding data from a subset of the file descriptors in fdarray. The subset is distrubted round robin style: where thread i reads the i + numthreads*k elements of fdarray. 


			fdarray
start->[1|2|3|4|1|2...3|4]<-end


At the beginning of main threads are blocking on a mutex as no connections have been made. Once a connection is made main will unlock the mutex and the respective thread will grab the mutex and start work. 

The main thread only listens and accepts new connections. 

A thread essentially just runs select() on its subset of file descriptors. Once an EOF signal has been received a thread closes the connection and atomically (since pthreads doesn't support atomic operations a mutex is used instead) updates the variable TOTAL_CONNECTIONS. We also have a time-out on select in case there is currently no work on existing connections and a new connection is loaded by main. 

Ignore: tests.c and tests (I didn't want to modify the Makefile);
Tuning: Servers are made to do different things. Discussion to come. 
TODO: Buffering
