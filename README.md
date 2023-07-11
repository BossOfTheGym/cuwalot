# cuwalot
Tree-based memory allocator. Utilizes red-black trees. The best allocator in the universe that is... under construction yet.

**TODO**
- change shitty codestyle
- remove design flaws
- rework cache_alloc
- add should_free_smd flag as dirty greedy hacky optimization
- remove stupid enums
- multithreading, rework pool_alloc
  - remove stupid allocation size check
  - fuck standart API, remove it completely
  - add bins instead of pools & bins or... whatever, yeah, unify the storage and and locking possibilities there
  - add option to add/remove locks for
    - pool_alloc
	- page_alloc
- README description
- decent unit testing
- Doxygen
- cmake install rules