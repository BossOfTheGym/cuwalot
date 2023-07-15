# cuwalot
Tree-based memory allocator. Utilizes red-black trees. The best allocator in the universe that is... under construction yet.

**TODO**
- remove TODO
- change shitty codestyle
- rework little bit alloc_traits: remove ugly repetition
- cppguidelines
- remove design flaws
- rework cache_alloc
- add should_free_smd flag as dirty greedy hacky optimization
- remove stupid enums
- fuck standart API: remove it completely (leave it to dinosaurs)
- fuck standart API: return memory handle(alloc_descr_t pointer) to facilitate memory freeing (it even removes neccessity in list_entry_t in alloc_descr, oh fuck)
- mmm! so good!
- multithreading, rework pool_alloc
  - remove stupid allocation size check
  - specify bin sizes using compile-time config
  - add bins instead of pools & bins or... whatever, yeah, unify the storage and and locking possibilities there
  - add option to add/remove locks for
    - pool_alloc
	- page_alloc
- README description
- decent unit testing
- Doxygen
- cmake install rules