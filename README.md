# cuwalot
Tree-based memory allocator. Utilizes red-black trees. The best allocator in the universe that is... under construction yet.

# TODO
**rework**
- page_alloc: add info blocks that will store info about initial allocated ranges
- rework page_alloc tests
- merge function in addr_cache and page_alloc

**testing**
- nothing to do

**complete TODO**
- testing in release mode
- multithreading
- test allocator
- decent unit testing
- page_alloc: densening of fbd's
- pool_alloc: densening of alloc_descr
- Doxygen
- README description
- cmake install rules
