# DynamicMemoryAllocator

A dynamic memory allocator in C.
This allocator is a explicit free list allocator with best-fit placement, immediate coalescing, and block splitting without splinters.
The blocks are inserted in sorted ascending address order.

The "sf_" in the beginning of these methods are there as a "bubble" so that things work in a special environment, created by the TAs for this class.
