# disruptor-mmap
Implementing a lock-free, infinite-length sequence for one-write and multiple-read operations across multiple processes based on mmap memory mapping.

基于mmap内存映射实现多进程一写多读无锁无限长度序列

SPMC一个进程内一个写入，多个进程多个读取，MPMC一个进程内多个写入，多个进程多个读取。
