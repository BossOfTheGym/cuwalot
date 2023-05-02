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

		static bool overlaps(fbd_t* fbd, void* ptr, std::size_t size) {
			auto l1 = (std::uintptr_t)fbd1->get_start();
			auto r1 = (std::uintptr_t)fbd1->get_end();
			auto l2 = (std::uintptr_t)ptr;
			auto r2 = (std::uintptr_t)advance_ptr(ptr, size);
			return l2 < r1 && l1 < r2;
		}

		static bool overlaps(fbd_t* fbd1, fbd_t* fbd2) {
			return overlaps(fbd1, fbd2->get_start(), fbd2->size)
		}

		static bool preceds(fbd_t* fbd1, fbd_t* fbd2) {
			return (std::uintptr_t)fbd1->get_start() < (std::uintptr_t)fbd2->get_start();
		}

		static fbd_t* addr_index_to_descr(addr_index_t* ptr) {
			return ptr ? base_to_obj(ptr, fbd_t, addr_index) : nullptr;
		}

		static fbd_t* size_index_to_descr(size_index_t* ptr) {
			return ptr ? base_to_obj(ptr, fbd_t, size_index) : nullptr;
		}

		struct addr_index_search_t : trb::implicit_key_t<addr_index_t> {
			bool compare(addr_index_t* node, void* start) const {
				auto block_start = (std::uintptr_t)addr_index_to_descr(node)->get_start();
				auto start_value = (std::uintptr_t)start;
				return block_start < start_value;
			}

			bool compare(addr_index_t* node1, addr_index_t* node2) const {
				return compare(node1, addr_index_to_descr(node2)->get_start());
			}
		};		

		struct size_index_search_t : trb::implicit_key_t<size_index_t> {
			// search first block with size greater than or equal to size
			bool compare(size_index_t* node, std::size_t size) const {
				return size_index_to_descr(node)->size < size;
			}

			bool compare(size_index_t* node1, size_index_t* node2) const {
				return compare(node1, size_index_to_descr(node2)->size);
			}
		};

		// for reversed search
		struct previous_block_search_t : trb::implicit_key_t<addr_index_t> {
			bool compare(addr_index_t* node, void* ptr) const {
				auto block_end = (std::uintptr_t)addr_index_to_descr(node)->get_end();
				auto ptr_value = (std::uintptr_t)ptr;
				return block_end > ptr_value;
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

	// memory_used(48): how much memory from this range is used, pretty weak guarantee 
	struct alignas(block_align) sysmem_descr_t {
		using smd_t = sysmem_descr_t;

		static smd_t* addr_index_to_descr(addr_index_t* ptr) {
			return ptr ? base_to_obj(ptr, smd_t, addr_index) : nullptr;
		}

		struct addr_index_search_t : trb::implicit_key_t<addr_index_t> {
			bool compare(addr_index_t* node, void* start) const {
				auto block_start = (std::uintptr_t)addr_index_to_descr(node)->get_start();
				auto start_value = (std::uintptr_t)start;
				return block_start < start_value;
			}

			bool compare(addr_index_t* node1, addr_index_t* node2) const {
				return compare(node1, addr_index_to_descr(node2)->get_start());
			}
		};

		struct containing_block_search_t : trb::implicit_key_t<addr_index_t> {
			// the result of the search must be checked that it contains searched pointer
			bool compare(addr_index_t* node, void* ptr) {
				auto block_end = (std::uintptr_t)addr_index_to_descr(node)->get_end();
				auto ptr_value = (std::uintptr_t)ptr;
				return ptr_value >= block_end;
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

		addr_index_t addr_index;
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
		void merge_smds(page_alloc_t& another) {
			addr_index_t* merge_addr_into = smd_addr;
			addr_index_t* merge_addr_from = another.smd_addr;
			if (smd_entry.get_count() < another.smd_entry.get_count()) {
				std::swap(merge_addr_from, merge_addr_into);
			}

			bst::traverse_inorder(merge_addr_from, [&] (addr_index_t* node) {
				merge_addr_into = trb::insert_lb(merge_addr_into, node, smd_t::addr_index_search_t{});
			});

			smd_entry.adopt(another.smd_entry);
			smd_addr = merge_addr_into;
			another.smd_addr = nullptr;
		}

		void merge_fbds(page_alloc_t& another) {
			std::size_t a = fbd_entry.get_count();
			std::size_t b = another.fbd_entry.get_count();
			
			fbd_entry.adopt(another.fbd_entry);
			if (a < b) {
				std::swap(fbd_addr, another.fbd_addr);
				std::swap(fbd_size, another.fbd_size);
			}

			bst::traverse_inorder(another.fbd_addr, [&] (addr_index_t* node) {
				fbd_t* fbd = fbd_t::addr_index_to_descr(node);
				void* ptr = fbd->get_start();
				std::size_t size = fbd->get_size();
				free_fbd(fbd, block_pool_release_mode_t::NoReinsertFree);
				insert_free_block(ptr, size);
			});

			another.fbd_addr = nullptr;
			another.fbd_size = nullptr;
		}

	public:
		void adopt(page_alloc_t& another) {
			if (this == &another) {
				return;
			}
			base_t::adopt(another);
			merge_smds(another);
			merge_fbds(another);
		}

		// mostly for debugging purposes
		void release_mem() {
			fbd_addr = nullptr;
			fbd_size = nullptr;
			fbd_entry.release_all([&] (void* block, std::size_t size) {
				base_t::deallocate(block, size);
				return true;
			});

			bst::traverse_inorder(smd_addr, [&] (addr_index_t* node) {
				smd_t* smd = smd_t::addr_index_to_descr(node);
				base_t::deallocate(smd->get_start(), smd->get_size());
			});
			smd_addr = nullptr;
			smd_entry.release_all([&] (void* block, std::size_t size) {
				base_t::deallocate(block, size);
				return true;
			});
		}

	private:
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
			if (bp_t* bp = smd_entry.release(smd)) {
				smd_entry.finish_release(bp, [&] (void* data, std::size_t size) {
					base_t::deallocate(data, size);
				});
			}
		}

	private:
		[[nodiscard]] smd_t* alloc_memory(std::size_t size) {
			size = align_value(size, page_size);

			smd_t* smd = alloc_smd(nullptr, size);
			if (!smd) {
				return nullptr;
			}

			smd->data = base_t::allocate(size);
			if (!smd->data) {
				free_smd(smd);
				return nullptr;
			}

			smd_addr = trb::insert_lb(smd_addr, &smd->addr_index, smd_t::addr_index_search_t{});
			return smd;
		}

		void free_memory(smd_t* smd) {
			base_t::deallocate(smd->data, smd->size);
			smd_addr = trb::remove(smd_addr, &smd->addr_index);
			free_smd(smd);
		}

	private:
		[[nodiscard]] fbd_t* alloc_fbd(void* data, std::size_t size) {
			if (fbd_t* fbd = fbd_entry.acquire(data, size)) {
				return fbd;
			}
			
			std::size_t pool_size = block_pool_size;
			void* pool_data = base_t::allocate(pool_size);
			if (pool_data) {
				fbd_entry.create_pool(pool_data, pool_size);
				return fbd_entry.acquire(data, size);
			} return nullptr;
		}

		void free_fbd(fbd_t* fbd, block_pool_release_mode_t mode = block_pool_release_mode_t::ReinsertFree) {
			if (bp_t* bp = fbd_entry.release(fbd, mode)) {
				fbd_entry.finish_release(bp, [&] (void* ptr, std::size_t size) {
					base_t::deallocate(ptr, size);
				});
			}
		}

	private:
		void* shrink_fbd_left(fbd_t* fbd, std::size_t size) {
			assert(size);
			assert(fbd->size >= size);

			void* ptr = fbd->get_start();
			fbd_size = trb::remove(fbd_size, &fbd->size_index);
			if (fbd->size != size) { // update block size by reinserting it into the free list
				fbd->shrink_left(size);
				fbd_size = trb::insert_lb(fbd_size, &fbd->size_index, fbd_t::size_index_search_t{});
			} else { // size will be zero, completely remove it from the free list
				fbd_addr = trb::remove(fbd_addr, &fbd->addr_index);
				free_fbd(fbd);
			} return ptr;
		}

		void* shrink_fbd_right(fbd_t* fbd, std::size_t size) {
			assert(size);
			assert(fbd->size >= size);

			void* ptr = advance_ptr(fbd->get_start(), fbd->size - size);
			fbd_size = trb::remove(fbd_size, &fbd->size_index);
			if (fbd->size != size) { // update block size by reinserting it into the free list
				fbd->shrink_right(size);
				fbd_size = trb::insert_lb(fbd_size, &fbd->size_index, fbd_t::size_index_search_t{});
			} else { // size will be zero, completely remove it from the free list
				fbd_addr = trb::remove(fbd_addr, &fbd->addr_index);
				free_fbd(fbd);
			} return ptr;
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
			auto [lb_node, ins_pos_node] = bst::search_insert_lb(fbd_addr, fbd_t::addr_index_search_t{}, ptr);

			fbd_t* ins_pos = fbd_t::addr_index_to_descr(ins_pos_node);
			fbd_t* right = fbd_t::addr_index_to_descr(lb_node);

			addr_index_t* left_node;
			if (!lb_node || lb_node != ins_pos_node) {
				left_node = ins_pos_node;
			} else {
				left_node = bst::predecessor(lb_node);
			} fbd_t* left = fbd_t::addr_index_to_descr(left_node);

			fbd_t dummy = { .size = size, .data = ptr };
			if (left && fbd_t::overlaps(&dummy, left) || right && fbd_t::overlaps(&dummy, right)) {
				std::abort(); // most probably double free
			}

			bool consumes_left = (left && left->get_end() == dummy.get_start());
			bool consumes_right = (right && dummy.get_end() == right->get_start());

			return {left, right, ins_pos, consumes_left, consumes_right};
		}

		// impl function
		// fbd can be dummy (created on stack)
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
		// O(4 * log(n)) at worst (insert_lb counts as 2 operations)
		// returns coalesced block, block has already been inserted into the addr index but not into the size index
		// so insert it into the size index at the end of operation
		fbd_t* coalesce_free_block(const coalesce_info_t& info, fbd_t* block, bool is_dummy) {
			fbd_t* coalesced_block = block;
			if (info.consumes_left) {
				fbd_size = trb::remove(fbd_size, &info.left->size_index);
				info.left->extend_right(coalesced_block->size);
				if (!is_dummy) {
					free_fbd(coalesced_block);
				} coalesced_block = info.left;
			}
			
			if (info.consumes_right) {
				fbd_size = trb::remove(fbd_size, &info.right->size_index);
				info.right->extend_left(coalesced_block->size);
				if (info.consumes_left) {
					fbd_addr = trb::remove(fbd_addr, &coalesced_block->addr_index);
					free_fbd(coalesced_block);
				} coalesced_block = info.right;
			}

			if (info.requires_fbd_alloc()) { // block must not be dummy, block is not in addr index, insert
				assert(!is_dummy);
				fbd_addr = trb::insert_lb_hint(fbd_addr, &coalesced_block->addr_index, &info.ins_pos->addr_index, fbd_t::addr_index_search_t{});
			} return coalesced_block;
		}

		// O(13*log(n) + walk), (insert_lb counts as 2 operations)
		void insert_free_block(void* ptr, std::size_t size) {
			fbd_t* coalesced_block = nullptr;
			coalesce_info_t info = get_coalesce_info(ptr, size);
			if (info.requires_fbd_alloc()) {
				fbd_t* fbd = alloc_fbd(ptr, size);
				if (!fbd) {
					std::abort();
				} coalesced_block = coalesce_free_block(info, fbd, false);
			} else {
				fbd_t dummy{ .size = size, .data = ptr };
				coalesced_block = coalesce_free_block(info, &dummy, true);
			}

			smd_t* curr_smd = smd_t::addr_index_to_descr(bst::lower_bound(smd_addr, smd_t::containing_block_search_t{}, ptr));
			if (!curr_smd || (std::uintptr_t)ptr < (std::uintptr_t)curr_smd->get_start()) {
				std::abort(); // no contanining sysmem region
			}

			auto coalesced_block_start = coalesced_block->get_start();
			auto coalesced_block_end = coalesced_block->get_end();
			auto cut_start = coalesced_block_end;
			auto cut_end = coalesced_block_start;
			while (curr_smd && (std::uintptr_t)curr_smd->start() < coalesced_block_end) {
				auto curr_smd_start = (std::uintptr_t)curr_smd->get_start();
				auto curr_smd_end = (std::uintptr_t)curr_smd->get_end();

				auto a = std::max(coalesced_block_start, curr_smd_start);
				auto b = std::min(coalesced_block_end, curr_smd_end);

				smd_t* next_smd = smd_t::addr_index_to_descr(bst::successor(&curr_smd->addr_index));
				if (a == curr_smd_start && b == curr_smd_end) {
					cut_start = std::min(cut_start, a);
					cut_end = std::max(cut_end, b);
					free_memory(curr_smd);
				} curr_smd = next_smd;
			}

			if (cut_start >= cut_end) {
				// no parts at all, inserting into the size index
				fbd_size = trb::insert_lb(fbd_size, fbd_t::size_index_search_t{}, &coalesced_block->size_index);
				return;
			}

			if (cut_start != coalesced_block_start) {
				// shrinking block so has the same size as the first part
				coalesced_block->shrink_right(coalesced_block_end - cut_start);
				fbd_size = trb::insert_lb(fbd_size, fbd_t::size_index_search_t{}, &coalesced_block->size_index);

				if (cut_end != coalesced_block_end) {
					// inserting the second part
					if (fbd_t* fbd = alloc_fbd((void*)cut_end, coalesced_block_end - cut_end);) {
						fbd_addr = trb::insert_lb(fbd_addr, fbd_t::addr_index_search_t{}, &fbd->addr_index_t);
						fbd_size = trb::insert_lb(fbd_size, fbd_t::size_index_search_t{}, &fbd->size_index_t);
					} else {
						std::abort();
					}
				}
			} else if (cut_end != coalesced_block_end) {
				// first part is missing, inserting the second part
				coalesced_block->shrink_left(cut_end - cut_start);
				fbd_size = trb::insert_lb(fbd_size, fbd_t::size_index_search_t{}, &coalesced_block->size_index);
			} else {
				// no parts, completely remove block from the index
				fbd_addr = trb::remove(fbd_addr, &coalesced_block->addr_index); 
			}
		}

		[[nodiscard]] void* bite_free_block(fbd_t* block, std::size_t size) {
			assert(is_aligned(size, page_size));
			return shrink_fbd_left(block, size);
		}

	private:
		[[nodiscard]] void* try_alloc_from_existing(std::size_t size) {
			assert(is_aligned(size, page_size));

			if (addr_index_t* found = bst::lower_bound(fbd_size, fbd_t::size_index_search_t{}, size)) {
				return bite_free_block(fbd_t::size_index_to_descr(found), size);
			} return nullptr;
		}

		// always allocates fbd for the remaining memory as a simplification
		// as a fallback tries to allocate smaller memory region in case of failure
		[[nodiscard]] void* try_alloc_by_extend(std::size_t size) {
			assert(is_aligned(size, page_size));

			std::size_t size_ext = std::max(size, min_block_size);

			smd_t* smd = alloc_memory(size_ext); 
			if (!smd) {
				smd = alloc_memory(size); // fallback
				if (!smd) {
					return nullptr;
				} size_ext = size;
			}

			void* rest_ptr = (char*)smd->data + size;
			std::size_t rest_size = size_ext - size;
			if (rest_size == 0) {
				return smd->data;
			}
			insert_free_block(rest_ptr, rest_size);
			return smd->data;
		}

		[[nodiscard]] void* try_alloc_memory(std::size_t size) {
			assert(is_aligned(size, page_size));
			if (void* ptr = try_alloc_from_existing(size)) {
				return ptr;
			} return try_alloc_by_extend(size);
		}

	public:
		[[nodiscard]] void* allocate(std::size_t size) {
			return try_alloc_memory(align_value(size, page_size));
		}

		// when we deallocate we check if we require fbd for that as in the case of heavy fragmentation so we don't waste
		// unneccessary fbds for that
		void deallocate(void* ptr, std::size_t size) {
			insert_free_block(ptr, align_value(size, page_size))
		}

		[[nodiscard]] void* reallocate(void* old_ptr, std::size_t old_size, std::size_t new_size) {
			std::size_t old_size_aligned = align_value(old_size, page_size);
			std::size_t new_size_aligned = align_value(new_size, page_size);

			if (new_size_aligned == old_size_aligned) {
				return old_ptr;
			}

			if (new_size_aligned < old_size_aligned) {
				void* old_ptr_end = (char*)old_ptr + new_size_aligned;
				std::size_t delta = old_size_aligned - new_size_aligned;
				insert_free_block(old_ptr_end, delta);
				return old_ptr;
			}

			// new_size_aligned > old_size_aligned
			// check if there is a block adjacent to right of the allocation
			if (addr_index_t* found = bst::lower_bound(fbd_addr, fbd_t::addr_index_search_t{}, old_ptr)) {
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

	public:
		// mostly for debugging purpose
		auto get_size_index() const {
			return bst::tree_wrapper_t{fbd_size};
		}

		// mostly for debugging purpose
		auto get_addr_index() const {
			return bst::tree_wrapper_t{fbd_addr};
		}

		std::size_t get_true_size(std::size_t size) {
			return align_value(size, page_size);
		}

		std::size_t get_page_size() const {
			return page_size;
		}

		std::size_t get_block_pool_size() const {
			return block_pool_size;
		}

		std::size_t get_min_block_size() const {
			return min_block_size;
		}

		std::size_t get_sysmem_pool_size() const {
			return sysmem_pool_size;
		}

	private:
		fbd_entry_t fbd_entry{};
		addr_index_t* fbd_addr{}; // free blocks stored by address
		size_index_t* fbd_size{}; // free blocks stored by size

		smd_entry_t smd_entry{};
		addr_index_t* smd_addr{}; // system memory blocks stored by address

		std::size_t page_size{};
		std::size_t block_pool_size{};
		std::size_t sysmem_pool_size{};
		std::size_t min_block_size{};
	};
}