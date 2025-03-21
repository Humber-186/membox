#pragma once
#include <cassert>
#include <cstdint>
#include <list>
#include <vector>

template <size_t elem_size = 4096> class BuddyAllocator {
public:
    using elem_idx_t = uint32_t;

    /**
     * @brief 构造函数
     * @param total_elems 总页数
     * @param max_order 最大阶数
     * @param elem_size 每页的大小
     */
    BuddyAllocator(elem_idx_t total_elems, uint8_t max_order);

    /**
     * @brief 分配一个2^order页大小的块
     * @param order 指定块的阶（页数为2^order）
     * @return 成功时返回页基址（非0），失败时返回0
     */
    uint64_t allocate(uint8_t order) { return allocate_idx(order) * elem_size; }

    /**
     * @brief 释放一个已分配的内存块
     * @param page_base 内存块的起始页地址
     * @param order 块的阶（页数为2^order）
     */
    void free(uint64_t page_base, uint8_t order) {
        assert(page_base % elem_size == 0);
        free_idx(page_base / elem_size, order);
    }

    /**
     * @brief 获取已分配出的内存大小
     * @return 已分配出的物理内存大小，单位为字节
     */
    size_t get_usage() const { return m_elem_usage * elem_size; }

private:
    const elem_idx_t total_pages;
    const uint8_t max_order;
    size_t m_elem_usage = 0;

    /**
     * @brief 内部函数：分配一个块，返回块的基址页号
     * @param order 指定块的阶
     * @return 成功时返回块的起始页号，失败时返回0
     */
    elem_idx_t allocate_idx(uint8_t order);

    /**
     * @brief 内部函数：释放指定块
     * @param block 块的起始页号
     * @param order 块的阶
     */
    void free_idx(elem_idx_t block, uint8_t order);

    // free_lists[i] 存放大小为2^i页的空闲块起始页号
    std::vector<std::list<elem_idx_t>> free_lists;

    /**
     * @brief 根据 buddy 系统规则，计算 buddy 块的索引
     * @param block 当前块的索引
     * @param order 当前块的阶
     * @return 对应的buddy块的索引
     */
    elem_idx_t get_buddy_idx(elem_idx_t block, uint8_t order) { return block ^ (1u << order); }
};
