#pragma once

#include "core.hpp"
#include "mem_api.hpp"
#include "alloc_tag.hpp"
#include "block_pool.hpp"
#include "alloc_traits.hpp"

namespace cuw::mem {
	// addr_index: store block in an address index so we can search it by address
	// size_index: store block in a size index os we can search it by size
	// offset(16): offset from prime block
	// size(48): size of the block in bytes (page_size aligned)
	// data: pointer to data
	struct alignas(block_align) free_block_descr_t {
		using fbd_t = free_block_descr_t;

		static bool overlaps(fbd_t* fbd1, fbd_t* fbd2) {
			auto l1 = (std::uintptr_t)fbd1->get_start();
			auto r1 = (std::uintptr_t)fbd1->get_end();
			auto l2 = (std::uintptr_t)fbd2->get_start();
			auto r2 = (std::uintptr_t)fbd2->get_end();
			return l2 < r1 && l1 < r2;
		}

		static bool preceds(fbd_t* fbd1, fbd_t* fbd2) {
			return (std::uintptr_t)fbd1->get_start() < (std::uintptr_t)fbd2->get_start();
		}

		static fbd_t* addr_index_to_descr(addr_index_t* ptr) {
			return base_to_obj(ptr, fbd_t, addr_index); // OOP is for suckers
		}

		static fbd_t* size_index_to_descr(size_index_t* ptr) {
			return base_to_obj(ptr, fbd_t, size_index); // OOP is for suckers
		}

		struct addr_ops_t : trb::implicit_key_t<addr_index_t> {
			// order blocks by block start
			bool compare(addr_index_t* node1, addr_index_t* node2) const {
				auto addr1 = (std::uintptr_t)addr_index_to_descr(node1)->get_start();
				auto addr2 = (std::uintptr_t)addr_index_to_descr(node2)->get_start();
				return addr1 < addr2; // non-intersecting ranges
			}

			// search first block containing ptr (upper_bound search)
			bool compare(addr_index_t* node, void* ptr) const {
				auto ptr_value = (std::uintptr_t)ptr;
				return ptr_value >= (std::uintptr_t)addr_index_to_descr(node)->get_end();
			}
		};

		struct size_ops_t : trb::implicit_key_t<size_index_t> {
			// order block by block size
			bool compare(size_index_t* node1, size_index_t* node2) const {
				return size_index_to_descr(node1)->size < size_index_to_descr(node2)->size;
			}

			// search first block with size greater than or equal to size
			bool compare(size_index_t* node, std::size_t size) const {
				return size_index_to_descr(node)->size < size;
			}
		};

		void* get_start() const {
			return data;
		}

		void* get_end() const {
			return (char*)data + size;
		}

		void extend_right(std::size_t amount) {
			size += amount;
		}

		void extend_left(std::size_t amount) {
			size += amount;
			data = (char*)data - amount;
		}

		void shrink_left(std::size_t amount) {
			size -= amount;
			data = (char*)data + amount;
		}

		void shrink_right(std::size_t amount) {
			size -= amount;
		}

		addr_index_t addr_index;
		size_index_t size_index;
		attrs_t offset:16, size:48;
		void* data;
	};

	static_assert(do_fits_block<free_block_descr_t>);

	struct alignas(block_align) sysmem_descr_t {
		using smd_t = sysmem_descr_t;

		static smd_t* addr_index_to_descr(addr_index_t* index) {
			return base_to_obj(index, smd_t, addr_index);
		}

		static smd_t* size_index_to_descr(size_index_t* index) {
			return base_to_obj(index, smd_t, addr_index);
		}

		struct addr_ops_t : trb::implicit_key_t<addr_index_t> {
			bool compare(addr_index_t* node1, addr_index_t* node2) const {
				auto data1 = (std::uintptr_t)addr_index_to_descr(node1)->get_start();
				auto data2 = (std::uintptr_t)addr_index_to_descr(node2)->get_start();
				return data1 < data2;
			}
		};

		struct size_ops_t : trb::implicit_key_t<size_index_t> {
			bool compare(size_index_t* node1, size_index_t* node2) const {
				return size_index_to_descr(node1)->size < size_index_to_descr(node2)->size;
			}
		};

		std::size_t get_size() const {
			return size;
		}

		void* get_start() const {
			return data;
		}

		void* get_end() const {
			return (char*)data + size;
		}

		addr_index_t addr_index; // can be used, for example, for tracking if region is free or not
		size_index_t size_index; // pretty useless now, can be thrown away, but exists
		attrs_t offset:16, size:48;
		void* data;
	};

	static_assert(do_fits_block<sysmem_descr_t>);

	template<class descr_t>
	class descr_entry_t : public block_pool_entry_t {
	public:
		using bp_t = block_pool_t;
		using base_t = block_pool_entry_t;

		static_assert(std::is_aggregate_v<descr_t>);

		descr_t* acquire(void* data, std::size_t size) {
			if (auto [ptr, offset] = base_t::acquire(); ptr) {
				++count;
				return new (ptr) descr_t { .offset = offset, .size = size, .data = data };
			} return nullptr;
		}

		bp_t* release(descr_t* fbd) {
			--count;
			return base_t::release(fbd, fbd->offset);
		}

		void adopt(descr_entry_t& another) {
			base_t::adopt(another);
			count += another.count;
			another.count = 0;
		} 

		std::size_t get_count() const {
			return count;
		}

	private:
		std::size_t count{};
	};

	using free_block_descr_entry_t = descr_entry_t<free_block_descr_t>;
	using sysmem_descr_entry_t = descr_entry_t<sysmem_descr_t>;

	// all allocations will be multiple of page_size
	// size is now size in bytes
	// size cannot be less than page_size or block_align
	// O(7 * log(n)) complexity at its finest on deallocation
	template<class basic_alloc_t>
	class page_alloc_t : public basic_alloc_t {
	public:
		using this_t = page_alloc_t;
		using base_t = basic_alloc_t;
		using fbd_t = free_block_descr_t;
		using smd_t = sysmem_descr_t;
		using fbd_entry_t = free_block_descr_entry_t;
		using smd_entry_t = sysmem_descr_entry_t;

		static_assert(has_sysmem_alloc_tag_v<base_t>);

		template<class ... args_t>
		page_alloc_t(args_t&& ... args) : base_t(std::forward<args_t>(args)...) {
			if constexpr(base_t::use_resolved_page_size) {
				page_size = base_t::alloc_page_size;
			} else {
				if (auto [info, status] = get_sysmem_info(); status == 0) {
					page_size = info.page_size;
				} else {
					page_size = base_t::alloc_page_size;
				}
			}
			block_pool_size = align_value(base_t::alloc_block_pool_size, page_size);
			sysmem_pool_size = align_value(base_t::alloc_sysmem_pool_size, page_size);
			min_block_size = align_value(base_t::alloc_min_block_size, page_size);
		}

		page_alloc_t(const page_alloc_t&) = delete;
		page_alloc_t(page_alloc_t&&) = delete;

		~page_alloc_t() {
			release_mem();
		}

		page_alloc_t& operator = (const page_alloc_t&) = delete;
		page_alloc_t& operator = (page_alloc_t&&) = delete;

	private:
		void merge_fbds_insertion(page_alloc_t& another) {
			fbd_entry.adopt(another.fbd_entry); // we now posess all fbd entries, so we can free them from this allocator
			bst::traverse_inorder(another.fbd_addr, [&] (addr_index_t* node) {
				fbd_t* fbd = fbd_t::addr_index_to_descr(node);
				insert_free_block(get_coalesce_info(fbd->data, fbd->size), fbd);
			});
			another.fbd_addr = nullptr;
			another.fbd_size = nullptr;
			release_empty_pools();
		}

		void merge_fbds_flatten(page_alloc_t& another) { 
			fbd_entry.adopt(another.fbd_entry); // we now posess all fbd entries, so we can free them from this allocator

			auto [curr1, curr1_tail] = bst::flatten(fbd_addr);
			auto [curr2, curr2_tail] = bst::flatten(another.fbd_addr);

			fbd_t* curr_fbd = nullptr;
			if (curr1 && curr2) {
				fbd_t* fbd1 = fbd_t::addr_index_to_descr(curr1);
				fbd_t* fbd2 = fbd_t::addr_index_to_descr(curr2);
				if (fbd_t::overlaps(fbd1, fbd2)) {
					std::abort();
				} if (fbd_t::preceds(fbd1, fbd2)) {
					curr_fbd = fbd1;
					curr1 = curr1->left;
				} else {
					curr_fbd = fbd2;
					curr2 = curr2->left;
				}
			} else if (curr1) {
				curr_fbd = fbd_t::addr_index_to_descr(curr1);
				curr1 = curr1->left;
			} else if (curr2){
				curr_fbd = fbd_t::addr_index_to_descr(curr2);
				curr2 = curr2->left;
			} else {
				release_empty_pools();
				return; // both allocators are empty
			}

			bst::head_tail_t<addr_index_t> merged;

			auto try_merge = [&] (fbd_t* dst, fbd_t* src) {
				if (dst->get_end() == src->get_start()) {
					dst->extend_right(src->size);
					return true;
				} return false;
			};

			auto add_fbd = [&] (fbd_t* fbd) {
				merged.append(&fbd->addr_index);
			};

			auto merge_fbd = [&] (fbd_t* fbd1, fbd_t* fbd2) {
				if (try_merge(fbd1, fbd2)) {
					release_fbd(fbd2);
					return fbd1;
				} else {
					add_fbd(fbd1);
					return fbd2;
				}
			};

			while (curr1 && curr2) {
				fbd_t* fbd1 = fbd_t::addr_index_to_descr(curr1);
				fbd_t* fbd2 = fbd_t::addr_index_to_descr(curr2);
				if (fbd_t::overlaps(fbd1, fbd2)) {
					std::abort();
				} if (fbd_t::preceds(fbd1, fbd2)) {
					curr1 = curr1->left;
					curr_fbd = merge_fbd(curr_fbd, fbd1);
				} else {
					curr2 = curr2->left;
					curr_fbd = merge_fbd(curr_fbd, fbd2);
				}
			} while (curr1) {
				fbd_t* fbd = fbd_t::addr_index_to_descr(curr1);
				curr1 = curr1->left;
				curr_fbd = merge_fbd(curr_fbd, fbd);
			} while (curr2) {
				fbd_t* fbd = fbd_t::size_index_to_descr(curr2);
				curr2 = curr2->left;
				curr_fbd = merge_fbd(curr_fbd, fbd);
			} add_fbd(curr_fbd);

			// rebuilding indices;
			addr_index_t* new_addr_index = nullptr;
			for (addr_index_t* curr = merged.head->left; curr; curr = curr->left) {
				fbd_t* fbd = fbd_t::addr_index_to_descr(curr);
				new_addr_index = trb::insert_lb(new_addr_index, &fbd->addr_index, fbd_t::addr_ops_t{});
			}

			size_index_t* new_size_index = nullptr;
			bst::traverse_inorder(new_addr_index, [&] (addr_index_t* node) {
				fbd_t* fbd = fbd_t::addr_index_to_descr(node);
				new_size_index = trb::insert_lb(new_size_index, &fbd->size_index, fbd_t::size_ops_t{});
			});

			fbd_addr = new_addr_index;
			fbd_size = new_size_index;
			another.fbd_addr = nullptr;
			another.fbd_size = nullptr;
			release_empty_pools();
		}

		void merge_fbds(page_alloc_t& another) {
			std::size_t a = fbd_entry.get_count();
			std::size_t b = another.fbd_entry.get_count();
			std::size_t d = (a < b ? b - a : a - b);
			std::size_t k = base_t::alloc_merge_coef;
			if ((a + b) / d > k) {
				merge_fbds_flatten(another);
			} else {
				merge_fbds_insertion(another);
			}
		}

		void merge_smds(page_alloc_t& another) {
			addr_index_t* merge_addr_into = smd_addr;
			size_index_t* merge_size_into = smd_size;
			addr_index_t* merge_addr_from = another.smd_addr;
			size_index_t* merge_size_from = another.smd_size;
			if (smd_entry.get_count() < another.smd_entry.get_count()) {
				std::swap(merge_addr_from, merge_addr_into);
				std::swap(merge_size_from, merge_size_into);
			}

			bst::traverse_inorder(merge_addr_from, [&] (addr_index_t* node) {
				merge_addr_into = trb::insert_lb(merge_addr_into, node, smd_t::addr_ops_t{});
			});
			bst::traverse_inorder(merge_size_from, [&] (size_index_t* node) {
				merge_size_into = trb::insert_lb(merge_size_into, node, smd_t::size_ops_t{});
			});

			smd_entry.adopt(another.smd_entry);
			smd_addr = merge_addr_into;
			smd_size = merge_size_into;
			another.smd_addr = nullptr;
			another.smd_size = nullptr;
		}

	public:
		// this function modifies index
		void adopt(page_alloc_t& another) {
			if (this == &another) {
				return;
			}
			base_t::adopt(another);
			merge_fbds(another);
			merge_smds(another);
		}

		// this function does not modify index
		// mostly for debugging purpose
		void release_mem() {
			fbd_addr = nullptr;
			fbd_size = nullptr;
			fbd_entry.release_all([&] (void* block, std::size_t size) { return true; });

			bst::traverse_inorder(smd_addr, [&] (size_index_t* node) {
				smd_t* smd = smd_t::size_index_to_descr(node);
				base_t::deallocate(smd->get_start(), smd->get_size());
				// we don't need to deallocate them properly
			});
			smd_addr = nullptr;
			smd_size = nullptr;
			smd_entry.release_all([&] (void* block, std::size_t size) {
				base_t::deallocate(block, size);
				return true;
			});
		}

		// this function does not modify index
		// mostly for debugging purpose
		auto get_size_index() const {
			return bst::tree_wrapper_t{fbd_size};
		}

		// this function does not modify index
		// mostly for debugging purpose
		auto get_addr_index() const {
			return bst::tree_wrapper_t{fbd_addr};
		}

	private:
		struct coalesce_info_t {
			bool requires_fbd_alloc() const {
				return !consumes_left && !consumes_right;
			}

			fbd_t* left{};
			fbd_t* right{};
			fbd_t* ins_pos{};
			bool consumes_left{};
			bool consumes_right{};
		};

		// this function does not modify index 
		// O(2*log(n)) at worst
		coalesce_info_t get_coalesce_info(void* ptr, std::size_t size) {
			assert(ptr);
			assert(is_aligned(size, page_size));

			// we use search_insert_lb because it will return both lower bound and insert position
			// in case if lower is nullptr we can use insert position
			// lower bound is always (if not nullptr) greater by address than searched block
			// (there can be no two blocks with the same address and address ranges do not intersect)
			// there can be the following cases:
			// 1) lb != nullptr => ins_pos != nullptr (we aditionally find predecessor, ins_pos is successor => we're good)
			// 2) lb == nullptr, ins_pos can be nullptr (tree is empty) or not (we find successor)
			// 3) that's it, no more cases
			fbd_t dummy = { .size = size, .data = ptr };
			auto [lb_node, ins_pos_node] = bst::search_insert_lb(fbd_addr, fbd_t::addr_ops_t{}, &dummy.addr_index);

			fbd_t* ins_pos = ins_pos_node ? fbd_t::addr_index_to_descr(ins_pos_node) : nullptr;
			fbd_t* right = lb_node ? fbd_t::addr_index_to_descr(lb_node) : nullptr;

			addr_index_t* left_node;
			if (!lb_node || lb_node != ins_pos_node) {
				left_node = ins_pos_node;
			} else {
				left_node = bst::predecessor(lb_node);
			} fbd_t* left = left_node ? fbd_t::addr_index_to_descr(left_node) : nullptr;

			if (left && fbd_t::overlaps(&dummy, left) || right && fbd_t::overlaps(&dummy, right)) {
				std::abort(); // most probably double free
			}

			bool consumes_left = (left && left->get_end() == dummy.get_start());
			bool consumes_right = (right && dummy.get_end() == right->get_start());

			return {left, right, ins_pos, consumes_left, consumes_right};
		}

		// ... in between theese calls allocate fbd in any way (if it's required) ...
		// ... and be careful when some code modifies index in between this calls as coalesce_info_t gets invalidated ...

		// this function modifies index
		// fbd must be dummy (must not be acquired by appropriate call but used via dummy variable on stack like in get_coalesce_info)
		//
		// if we dont require an fbd allocation(is_dummy can be true):
		// coalesced_info_t info = ...;
		// fbd_t dummy = {.size = size, .data = data};
		// insert_free_block(info, &dummy)
		//
		// if we require an fbd allocation(is_dummy must be false):
		// coalesced_info_t info = ...;
		// fbd_t* block = function_to_allocate_fbd(data, size);
		// insert_free_block(info, block)
		//
		// block must not be in index before function call
		// O(5 * log(n)) at worst (insert_lb counts as 2 operations)
		void insert_free_block(const coalesce_info_t& info, fbd_t* block, bool is_dummy) {
			fbd_t* coalesced_block = block;
			if (info.consumes_left) {
				fbd_size = trb::remove(fbd_size, &info.left->size_index);
				info.left->extend_right(coalesced_block->size);
				if (!is_dummy) {
					release_fbd(coalesced_block);
				} coalesced_block = info.left;
			}
			
			if (info.consumes_right) {
				fbd_size = trb::remove(fbd_size, &info.right->size_index);
				info.right->extend_left(coalesced_block->size);
				if (info.consumes_left) {
					fbd_addr = trb::remove(fbd_addr, &coalesced_block->addr_index);
					release_fbd(coalesced_block);
				} coalesced_block = info.right;
			}

			fbd_size = trb::insert_lb(fbd_size, &coalesced_block->size_index, fbd_t::size_ops_t{});
			if (info.requires_fbd_alloc()) { // block must not be dummy, block is not in addr index
				assert(!is_dummy);
				fbd_addr = trb::insert_lb_hint(fbd_addr, &coalesced_block->addr_index, &info.ins_pos->addr_index, fbd_t::addr_ops_t{});
			}
		}

		// this function modifies index
		// inserted block must be true allocated fbd
		void insert_free_block(const coalesce_info_t& info, fbd_t* block) {
			insert_free_block(info, block, false);
		}

		// this function modifies index
		// block will coalesce with some of the neighbours, we can use dummy fbd 
		void insert_free_block(const coalesce_info_t& info, void* data, std::size_t size) {
			fbd_t dummy{ .size = size, .data = data };
			insert_free_block(info, &dummy, true);
		}

	private:
		// this function does not modify index
		[[nodiscard]] fbd_t* acquire_fbd(void* data, std::size_t size) {
			return fbd_entry.acquire(data, size);
		}

		// this function does not modify index
		void release_fbd(fbd_t* fbd) {
			fbd_entry.release(fbd);
		}

		// this function does not modify index
		void extend_fbd(void* block_ptr, std::size_t size) {
			fbd_entry.create_pool(block_ptr, size);
		}

		// this function modifies index
		void release_empty_pools() {
			// we try this only once, it is possible that some of the blocks can be freed without fbd allocation
			// after the first pass etc..
			fbd_entry.release_empty([&] (void* ptr, std::size_t size) {
				if (auto info = get_coalesce_info(ptr, size); !info.requires_fbd_alloc()) {
					insert_free_block(info, ptr, size);
					return true;
				} return false;
			});

			while (true) {
				fbd_t* fbd = acquire_fbd(nullptr, 0);
				if (!fbd) {
					break;
				}

				bool popped = fbd_entry.pop_empty([&] (void* ptr, std::size_t size) {
					fbd->size = size;
					fbd->data = ptr;
					insert_free_block(get_coalesce_info(ptr, size), fbd);
					return true;
				});
				if (!popped) {
					release_fbd(fbd);
					break;
				}
			}
		}

		[[nodiscard]] smd_t* alloc_smd(void* data, std::size_t size) {
			if (smd_t* smd = smd_entry.acquire(data, size)) {
				return smd;
			}

			std::size_t pool_size = sysmem_pool_size;
			void* pool_data = base_t::allocate(pool_size);
			if (!pool_data) {
				return nullptr;
			}
			smd_entry.create_pool(pool_data, pool_size);
			return smd_entry.acquire(data, size);
		}

		void free_smd(smd_t* smd) {
			smd_entry.release(smd);
		}

		[[nodiscard]] smd_t* alloc_memory(std::size_t size) {
			size = align_value(size, page_size);

			void* memory = base_t::allocate(size);
			if (!memory) {
				return nullptr;
			}

			smd_t* smd = alloc_smd(memory, size);
			if (!smd) {
				base_t::deallocate(memory, size);
				return nullptr;
			}
				
			smd_addr = trb::insert_lb(smd_addr, &smd->addr_index, smd_t::addr_ops_t{});
			smd_size = trb::insert_lb(smd_size, &smd->size_index, smd_t::size_ops_t{});
			return smd;
		}

		void free_memory(smd_t* smd) {
			base_t::deallocate(smd->data, smd->size);
			smd_addr = trb::remove(smd_addr, &smd->addr_index);
			smd_size = trb::remove(smd_size, &smd->size_index);
			free_smd(smd);
		}

	private:
		// this function modifies index
		// fbd is already in the index
		[[nodiscard]] void* bite_free_block(fbd_t* block, std::size_t size) {
			assert(is_aligned(size, page_size));
			assert(block->size >= size);

			void* ptr = block->get_start();
			fbd_size = trb::remove(fbd_size, &block->size_index);
			if (block->size != size) { // update block size by reinserting it into the free list
				block->shrink_left(size);
				fbd_size = trb::insert_lb(fbd_size, &block->size_index, fbd_t::size_ops_t{});
			} else { // size will be zero, completely remove it from the free list
				fbd_addr = trb::remove(fbd_addr, &block->addr_index);
				release_fbd(block);
			} return ptr;
		}

		// this function modifies index
		[[nodiscard]] void* try_alloc_from_existing(std::size_t size) {
			assert(is_aligned(size, page_size));

			if (addr_index_t* found = bst::lower_bound(fbd_size, fbd_t::size_ops_t{}, size)) {
				return bite_free_block(fbd_t::size_index_to_descr(found), size);
			} return nullptr;
		}

		// this function modifies index
		// always allocates fbd for the remaining memory as a simplification
		// as a fallback tries to allocate smaller memory region in case of failure
		[[nodiscard]] void* try_alloc_by_extend(std::size_t size) {
			assert(is_aligned(size, page_size));

			// we guarantee that we always have space for additional block pool
			std::size_t size_ext = std::max(size, min_block_size + block_pool_size);

			smd_t* smd = alloc_memory(size_ext); 
			if (!smd) {
				smd = alloc_memory(size); // fallback
				if (!smd) {
					return nullptr;
				} size_ext = size;
			}

			void* ptr = smd->data;

		one_more_time:
			void* rest_ptr = (char*)ptr + size;
			std::size_t rest_size = size_ext - size;
			if (rest_size == 0) {
				return ptr;
			} if (fbd_t* fbd = acquire_fbd(rest_ptr, rest_size)) {
				insert_free_block(get_coalesce_info(rest_ptr, rest_size), fbd);
				return ptr;
			} else {
				extend_fbd(ptr, block_pool_size); // cut from the beginning
				ptr = (char*)ptr + block_pool_size;
				size_ext -= block_pool_size;
				goto one_more_time;
			}
		}

		// this function modifies index
		[[nodiscard]] void* try_alloc_memory(std::size_t size) {
			assert(is_aligned(size, page_size));

			if (void* ptr = try_alloc_from_existing(size)) {
				return ptr;
			} return try_alloc_by_extend(size);
		}

		// this function modifies index
		// as a fallback tries to allocate smaller memory region in case of failure
		[[nodiscard]] bool try_extend_fbd() {
			std::size_t size = block_pool_size;
			std::size_t size_ext = std::max(size, min_block_size + block_pool_size);
			
			smd_t* smd = alloc_memory(size_ext);
			if (!smd) {
				smd = alloc_memory(size); // fallback
				if (!smd) {
					return false;
				} size_ext = size;
			}

			void* ptr = smd->data;
			extend_fbd(ptr, size); // cut from the beginning

			void* rest_ptr = (char*)ptr + size;
			std::size_t rest_size = size_ext - size;
			if (rest_size != 0) {
				auto info = get_coalesce_info(rest_ptr, rest_size);
				auto fbd = acquire_fbd(rest_ptr, rest_size); // guaranteed to exist
				insert_free_block(info, fbd);
			} return true;
		}

		// this function modifies index
		[[nodiscard]] fbd_t* try_alloc_fbd(void* data, std::size_t size) {
			assert(data);
			assert(is_aligned(size, page_size));

			if (fbd_t* fbd = acquire_fbd(data, size)) {
				return fbd;
			} if (void* block_pool = try_alloc_from_existing(block_pool_size)) {
				extend_fbd(block_pool, block_pool_size);
				return acquire_fbd(data, size); // must be non-nullptr
			} if (try_extend_fbd()) {
				return acquire_fbd(data, size); // must be non-nullptr 
			} return nullptr;
		}

	public:
		// this function modifies index
		[[nodiscard]] void* allocate(std::size_t size) {
			return try_alloc_memory(align_value(size, page_size));
		}

		// this function modifies index
		// when we deallocate we check if we require fbd for that as in the case of heavy fragmentation so we don't waste
		// unneccessary fbds for that
		void deallocate(void* ptr, std::size_t size) {
			size = align_value(size, page_size);
			if (auto info = get_coalesce_info(ptr, size); info.requires_fbd_alloc()) {
				if (fbd_t* fbd = try_alloc_fbd(ptr, size)) {
					insert_free_block(get_coalesce_info(ptr, size), fbd); // info can expire
				} else {
					std::abort(); // we're fucked :D
				}
			} else {
				insert_free_block(info, ptr, size);
			}
		}

		// this function modifies index
		[[nodiscard]] void* reallocate(void* old_ptr, std::size_t old_size, std::size_t new_size) {
			std::size_t old_size_aligned = align_value(old_size, page_size);
			std::size_t new_size_aligned = align_value(new_size, page_size);

			if (new_size_aligned == old_size_aligned) {
				return old_ptr;
			}

			if (new_size_aligned < old_size_aligned) {
				void* old_ptr_end = (char*)old_ptr + new_size_aligned;
				std::size_t delta = old_size_aligned - new_size_aligned;
				if (auto info = get_coalesce_info(old_ptr_end, delta); info.requires_fbd_alloc()) {
					if (fbd_t* fbd = try_alloc_fbd(old_ptr_end, delta)) {
						insert_free_block(get_coalesce_info(old_ptr_end, delta), fbd);
						return old_ptr;
					} return nullptr;
				} else {
					insert_free_block(info, old_ptr_end, delta);
					return old_ptr;
				}
			}

			// new_size_aligned > old_size_aligned
			// check if there is a block adjacent to right of the allocation
			if (addr_index_t* found = bst::lower_bound(fbd_addr, fbd_t::addr_ops_t{}, old_ptr)) {
				fbd_t* block = fbd_t::addr_index_to_descr(found);
				void* old_ptr_end = (char*)old_ptr + old_size_aligned;
				std::size_t delta = new_size_aligned - old_size_aligned;
				if (old_ptr_end == block->get_start() && block->size >= delta) {
					void* dummy = bite_free_block(block, delta); // next allocated block is neighbour to us
					return old_ptr;
				}
			} if (void* new_ptr = this_t::allocate(new_size)) {
				std::memcpy(new_ptr, old_ptr, old_size);
				this_t::deallocate(old_ptr, old_size);
				return new_ptr;
			} return nullptr;
		}

		// this function does not modify index
		std::size_t get_true_size(std::size_t size) {
			return align_value(size, page_size);
		}

		// this function does not modify index
		std::size_t get_page_size() const {
			return page_size;
		}

		// this function does not modify index
		std::size_t get_block_pool_size() const {
			return block_pool_size;
		}

		// this function does not modify index
		std::size_t get_min_block_size() const {
			return min_block_size;
		}

		// this function does not modify index
		std::size_t get_sysmem_pool_size() const {
			return sysmem_pool_size;
		}

	private:
		fbd_entry_t fbd_entry{};
		addr_index_t* fbd_addr{}; // free blocks stored by address
		size_index_t* fbd_size{}; // free blocks stored by size

		smd_entry_t smd_entry{};
		addr_index_t* smd_addr{}; // system memory blocks stored by address
		size_index_t* smd_size{}; // system memory blocks stored by size

		std::size_t page_size{};
		std::size_t block_pool_size{};
		std::size_t sysmem_pool_size{};
		std::size_t min_block_size{};
	};
}