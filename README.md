# disruptor-mmap
Implementing a lock-free, infinite-length sequence for one-write and multiple-read operations across multiple processes based on mmap memory mapping.

SPMC (Single Producer Multiple Consumer) refers to a scenario where there is a single producer process writing data to a shared memory buffer while multiple consumer processes read from it.

MPMC (Multiple Producer Multiple Consumer) refers to a scenario where multiple producer processes write data to a shared memory buffer while multiple consumer processes read from it.

基于mmap内存映射实现多进程一写多读无锁无限长度序列

SPMC一个进程内一个写入，多个进程多个读取，MPMC一个进程内多个写入，多个进程多个读取。
