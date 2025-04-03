#pragma once
#include "physical_mem.hpp"
#include <cstdint>
#include <memory>
#include <spdlog/logger.h>
#include <utility>

/**
 * @brief SV32基本页表翻译（不修改页表），可作为对硬件MMU的行为模拟
 */
class SV32_basic { // basic SV32 MMU (never change pagetable)
public:
    using paddr_t = PhysicalMemory::paddr_t;
    using vaddr_t = uint32_t;
    using pte_t = uint32_t;
    using pagetable_t = paddr_t;
    // static constexpr uint64_t PMEM_SIZE = (1ull << 34ull);
    // static constexpr uint64_t VMEM_SIZE = (1ull << 32ull);
    static constexpr size_t PAGESIZE = 4096;
    static constexpr int LEVELS = 2;

    SV32_basic(
        std::shared_ptr<PhysicalMemory> pmem, std::shared_ptr<spdlog::logger> logger = nullptr
    );

    /**
     * @brief 根据SV32页表机制，将虚拟地址转换为物理地址
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
     * @return 若成功则返回目标虚拟地址dst，失败返回0（这与C memcpy不同）
     * @note 这并非硬件MMU功能，仅为方便测试
     */
    vaddr_t memcpy(pagetable_t pagetable_root, vaddr_t dst, const void *src, size_t size) const;

    /**
     * @brief 将数据从虚拟地址空间复制到物理目的缓冲区（读操作）
     * @param pagetable_root 根页表物理地址
     * @param dst 主机端目的缓冲区
     * @param src 源数据的虚拟地址
     * @param size 要复制的字节数
     * @return 若成功则返回主机端目标缓冲区的指针dst，失败返回nullptr（这与C memcpy不同）
     * @note 这并非硬件MMU功能，仅为方便测试
     */
    void *memcpy(pagetable_t pagetable_root, void *dst, vaddr_t src, size_t size) const;

    // SV32标准中定义的虚拟地址、物理地址、页表项的位域
    struct BITRANGE {
        struct VA {
            static constexpr std::pair<uint8_t, uint8_t> PAGEOFFSET = {11, 00};
            static constexpr std::pair<uint8_t, uint8_t> VPN0 = {21, 12};
            static constexpr std::pair<uint8_t, uint8_t> VPN1 = {31, 22};
            static constexpr std::pair<uint8_t, uint8_t> VPN[LEVELS] = {VPN0, VPN1};
        };
        struct PA {
            static constexpr std::pair<uint8_t, uint8_t> PAGEOFFSET = {11, 00};
            static constexpr std::pair<uint8_t, uint8_t> PPNFULL = {33, 12};
            static constexpr std::pair<uint8_t, uint8_t> PPN0 = {20, 12};
            static constexpr std::pair<uint8_t, uint8_t> PPN1 = {29, 21};
            static constexpr std::pair<uint8_t, uint8_t> PPN[LEVELS] = {PPN0, PPN1};
        };
        struct PTE {
            static constexpr std::pair<uint8_t, uint8_t> V = {0, 0};
            static constexpr std::pair<uint8_t, uint8_t> R = {1, 1};
            static constexpr std::pair<uint8_t, uint8_t> W = {2, 2};
            static constexpr std::pair<uint8_t, uint8_t> X = {3, 3};
            static constexpr std::pair<uint8_t, uint8_t> U = {4, 4};
            static constexpr std::pair<uint8_t, uint8_t> G = {5, 5};
            static constexpr std::pair<uint8_t, uint8_t> A = {6, 6};
            static constexpr std::pair<uint8_t, uint8_t> D = {7, 7};
            static constexpr std::pair<uint8_t, uint8_t> XWR = {3, 1};
            static constexpr std::pair<uint8_t, uint8_t> RSW = {9, 8};
            static constexpr std::pair<uint8_t, uint8_t> PPNFULL = {31, 10};
            static constexpr std::pair<uint8_t, uint8_t> PPN0 = {19, 10};
            static constexpr std::pair<uint8_t, uint8_t> PPN1 = {31, 20};
            static constexpr std::pair<uint8_t, uint8_t> PPN[LEVELS] = {PPN0, PPN1};
        };
    };

    // 位操作工具函数
    // 从data中提取给定位域range范围的值
    static uint64_t bits_extract(uint64_t data, std::pair<uint8_t, uint8_t> range);
    // 将data_raw中给定位域range范围的值设置为value
    static uint64_t bits_set(
        uint64_t value, std::pair<uint8_t, uint8_t> range, uint64_t data_raw = 0
    );

protected:
    std::shared_ptr<PhysicalMemory> pmem;
    std::shared_ptr<spdlog::logger> logger = nullptr;
};
