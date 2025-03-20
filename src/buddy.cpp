#include "buddy.hpp"
#include <algorithm>
#include <cassert>

template <size_t elem_size>
BuddyAllocator<elem_size>::BuddyAllocator(elem_idx_t total_pages, uint8_t max_order)
    : total_pages(total_pages), max_order(max_order) {
    assert(total_pages > 1);
    assert(total_pages % (1ull << max_order) == 0); // 目前不这样会导致0号页不能提早移除
    free_lists.resize(max_order + 1);
    elem_idx_t i = 0;
    for (int order = max_order; order >= 0; order--) {
        while (i <= total_pages - (1u << order)) {
            free_lists[order].push_back(i);
            i += (1u << order);
        }
    }
    // 提前将0号页分配出并不释放，则之后分配操作返回0表示失败
    assert(allocate(0) == 0);
    m_elem_usage -= 1; // 0号页不计入使用量
}

template <size_t elem_size>
BuddyAllocator<elem_size>::elem_idx_t BuddyAllocator<elem_size>::allocate_idx(const uint8_t order) {
    if (order > max_order) return 0;

    uint8_t current_order = order;
    while (current_order <= max_order && free_lists[current_order].empty()) {
        current_order++;
    }
    if (current_order > max_order) return 0;

    elem_idx_t block = free_lists[current_order].front();
    free_lists[current_order].pop_front();
    while (current_order > order) {
        current_order--;
        elem_idx_t buddy = block + (1u << current_order);
        free_lists[current_order].push_back(buddy);
    }
    m_elem_usage += (1u << order);
    return block;
}

template <size_t elem_size>
void BuddyAllocator<elem_size>::free_idx(elem_idx_t block, const uint8_t order) {
    uint8_t cur_order = order;
    while (cur_order < max_order) {
        elem_idx_t buddy = get_buddy_idx(block, cur_order);
        auto &flist = free_lists[cur_order];
        auto it = std::find(flist.begin(), flist.end(), buddy);
        if (it == flist.end()) {
            break;
        }
        flist.erase(it);
        if (buddy < block) block = buddy;
        cur_order++;
    }
    free_lists[cur_order].push_back(block);
    assert(m_elem_usage >= (1u << order));
    m_elem_usage -= (1u << order);
}

template class BuddyAllocator<>;
