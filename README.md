# cuwalot
Tree-based memory allocator. Utilizes red-black trees. The best allocator in the universe that is... under construction yet.

# TODO
**rework**
- page_alloc: acquire/release memory range
- page_alloc: free system memory range if it is completely free
- page_alloc: free fbds if completely free
- page_alloc tests

**complete TODO**
- multithreading
- README description
- test allocator
- testing in release mode
- decent unit testing
- Doxygen
- cmake install rules

**somewhat good TODO**
- page_alloc: densening of fbd's
- pool_alloc: densening of alloc_descr
