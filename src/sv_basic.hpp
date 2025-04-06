#pragma once
#include "physical_mem.hpp"
#include <cstddef>
#include <cstdint>

template <typename Trait> class SV_basic {
public:
    using paddr_t = typename PhysicalMemoryInterface::paddr_t;
    using vaddr_t = typename Trait::vaddr_t;
    using pte_t = typename Trait::pte_t;
    using pagetable_t = paddr_t;
    using BITRANGE = typename Trait::BITRANGE;
    static constexpr int LEVELS = Trait::LEVELS;
    static constexpr size_t PAGESIZE = 4096;

    SV_basic(
        std::shared_ptr<PhysicalMemoryInterface> pmem,
        std::shared_ptr<spdlog::logger> logger = nullptr
    );

    /**
     * @brief 根据SV页表机制，将虚拟地址转换为物理地址
     * @param pagetable_root 根页表的物理地址
     * @param vaddr 要转换的虚拟地址
     * @return 转换后的物理地址(非0)，若转换失败则返回0
     */
    paddr_t translate(pagetable_t pagetable_root, vaddr_t vaddr) const;

    /**
     * @brief 将数据从主机端复制到虚拟地址空间（写操作）
     * @param pagetable_root 页表根物理地址
     * @param dst 目标虚拟地址
     * @param src 主机端源数据
     * @param size 要复制的字节数
     * @return 成功则返回目标虚拟地址dst，失败返回0（这与C memcpy不同）
     * @note 这并非硬件MMU功能，仅为方便测试
     */
    vaddr_t memcpy(pagetable_t pagetable_root, vaddr_t dst, const void *src, size_t size) const;

    /**
     * @brief 将数据从虚拟地址空间复制到物理目的缓冲区（读操作）
     * @param pagetable_root 根页表物理地址
     * @param dst 主机端目的缓冲区
     * @param src 源数据的虚拟地址
     * @param size 要复制的字节数
     * @return 成功则返回主机端目标缓冲区的指针dst，失败返回nullptr（这与C memcpy不同）
     * @note 这并非硬件MMU功能，仅为方便测试
     */
    void *memcpy(pagetable_t pagetable_root, void *dst, vaddr_t src, size_t size) const;

protected:
    std::shared_ptr<PhysicalMemoryInterface> pmem;
    std::shared_ptr<spdlog::logger> logger = nullptr;

    // 位操作工具函数
    // 从data中提取给定位域range范围的值
    static uint64_t bits_extract(uint64_t data, std::pair<uint8_t, uint8_t> range);
    // 将data_raw中给定位域range范围的值设置为value
    static uint64_t bits_set(
        uint64_t value, std::pair<uint8_t, uint8_t> range, uint64_t data_raw = 0ull
    );
};
