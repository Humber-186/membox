#pragma once
#include "buddy.hpp"
#include "physical_mem.hpp"
#include "sv32_basic.hpp"
#include <vector>

/**
 * @brief SV32 supervisor模式下的页表管理器（软件）
 * @note 利用buddy allocator管理物理内存，实现页表管理与类linux的mmap和munmap函数
 */
class SV32_supervisor : public SV32_basic {
public:
    SV32_supervisor(
        std::shared_ptr<PhysicalMemoryInterface> pmem, std::shared_ptr<spdlog::logger> logger = nullptr
    );

    /**
     * @brief 分配一个连续的虚拟地址空间，返回其起始地址
     * @param pagetable_root 页表根的物理地址，由create_pagetable()返回
     * @param vaddr 要分配的虚拟地址空间的起始地址，只是推荐值，实际分配的地址可能会不同
     * @param size 要分配的虚拟地址空间的大小，单位为字节。若size不是页大小的整数倍，则向上取整
     * @return 最终成功分配的虚拟地址，失败时返回0。成功分配的虚拟地址必定是页对齐的
     * @note 行为参照linux内核提供的mmap函数
     */
    vaddr_t mmap(pagetable_t pagetable_root, vaddr_t vaddr, size_t size);

    /**
     * @brief 释放由mmap分配的虚拟地址空间
     * @param pagetable_root 页表根的物理地址，由create_pagetable()返回
     * @param vaddr 要释放的虚拟地址空间的起始地址，应为mmap函数的返回值
     * @param size 要释放的虚拟地址空间的大小，单位为字节，应与调用mmap时匹配
     * @return 成功时返回0，失败时返回-1
     */
    int munmap(pagetable_t pagetable_root, vaddr_t vaddr, size_t size);

    /**
     * @brief 创建根页表
     * @return 创建成功将返回根页表的物理地址，失败返回0
     */
    pagetable_t create_pagetable();

    /**
     * @brief 销毁指定的根页表和所有下级页表，同时释放页表中所有映射的虚拟内存页。
     * @param pagetable_root 页表根的物理地址，由create_pagetable()返回。
     * @return 成功时返回0，失败时返回-1。
     */
    int destroy_pagetable(pagetable_t pagetable_root);

    /**
     * @brief 获取当前虚拟内存使用量
     * @return 当前虚拟内存使用量，单位为字节
     */
    size_t get_vmem_usage() const { return m_vpage_usage * PAGESIZE; }
    /**
     * @brief 获取当前物理内存使用量
     * @return 当前物理内存使用量，单位为字节
     */
    size_t get_pmem_usage() const { return buddy.get_usage(); }

private:
    // buddy allocator for physical memory management
    BuddyAllocator<PAGESIZE> buddy;

    // Virtual memory usage statistics (sum of all virtual address spaces)
    uint64_t m_vpage_usage = 0;

    // For debug assert
    std::vector<pagetable_t> m_ptroots; // all root-pagetables created
    void assert_ptroot(pagetable_t ptroot);

    // 新分配一个虚拟页，将其加入页表中，需要时会分配页表页
    int alloc_one_page(pagetable_t pagetable_root, vaddr_t vaddr);
    // 释放一个虚拟页，目前不会释放页表页
    int free_one_page(pagetable_t pagetable_root, vaddr_t vaddr);
    // 销毁一个页表，递归销毁所有下级页表
    int destroy_pagetable_one_level(pagetable_t ptaddr, int level);
};