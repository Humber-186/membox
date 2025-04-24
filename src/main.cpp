#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include "physical_mem.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <random>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

template <typename SV_basic, typename SV_supervisor>
int test(std::shared_ptr<spdlog::logger> logger) {
    std::shared_ptr<PhysicalMemoryInterface> pmem =
        std::make_shared<PhysicalMemoryBasicSim>((1ull << 32), logger);
    auto sv = std::make_shared<SV_supervisor>(pmem, logger);
    auto mmu = std::make_shared<SV_basic>(pmem, logger);

    constexpr size_t PAGESIZE = SV_basic::PAGESIZE;
    using pagetable_t = typename SV_basic::pagetable_t;
    using vaddr_t = typename SV_basic::vaddr_t;
    using paddr_t = typename SV_basic::paddr_t;

    //
    // --- 测试基本功能 ---
    //

    pagetable_t vmem1 = sv->create_pagetable();
    if (vmem1 == 0) {
        SPDLOG_LOGGER_ERROR(logger, "Basic test: create pagetable failed");
        return -1;
    }
    vaddr_t vaddr1 = 0x1000;
    char data[] = "Hello, World!";
    vaddr1 = sv->mmap(vmem1, vaddr1, sizeof(data));
    mmu->memcpy(vmem1, vaddr1, data, sizeof(data));

    paddr_t paddr1 = mmu->translate(vmem1, vaddr1);
    char data_read_out[128];
    if (!mmu->memcpy(vmem1, data_read_out, vaddr1, sizeof(data))) {
        SPDLOG_LOGGER_ERROR(logger, "Basic test: memcpy failed");
        return -1;
    }
    SPDLOG_LOGGER_DEBUG(
        logger, "Basic test: vaddr1=0x{:x}, paddr1=0x{:x}, data_read_out={}", vaddr1, paddr1,
        data_read_out
    );
    sv->munmap(vmem1, vaddr1, sizeof(data));
    sv->destroy_pagetable(vmem1);

    //
    // --- 随机测试 ---
    //

    std::srand(std::time(nullptr));
    std::uniform_real_distribution<double> randf64(0, 100);
    std::default_random_engine re;

    std::map<pagetable_t, std::map<vaddr_t, std::vector<uint8_t>>> goldModels;

    // 初始化一些虚拟地址空间，每个空间中写入5段随机数据
    for (int i = 0; i < 5; i++) {
        auto vmem = sv->create_pagetable();
        if (vmem != 0) {
            goldModels[vmem] = {}; // 新增虚拟地址空间与其gold model映射
            for (int j = 0; j < 5; j++) {
                vaddr_t vaddr = (std::rand() % 1000) * PAGESIZE; // 页对齐地址
                size_t dataSize = 1 + std::rand() % 8192;        // 数据大小
                vaddr = sv->mmap(vmem, vaddr, dataSize);
                if (vaddr == 0) {
                    SPDLOG_LOGGER_WARN(logger, "Init WrData mmap refused");
                    continue;
                }
                // 生成随机数据
                std::vector<uint8_t> testData(dataSize);
                for (size_t k = 0; k < dataSize; k++) {
                    testData[k] = static_cast<uint8_t>(std::rand());
                }
                // 写数据到虚拟内存
                if (!mmu->memcpy(vmem, vaddr, testData.data(), dataSize)) {
                    SPDLOG_LOGGER_WARN(logger, "Init WrData memcpy failed");
                    continue;
                }
                // 更新gold model
                goldModels[vmem][vaddr] = testData;
                SPDLOG_LOGGER_DEBUG(
                    logger, "Init WrData VMEM {:x}, vaddr 0x{:x}, size {}", vmem, vaddr, dataSize
                );
            }
        } else {
            SPDLOG_LOGGER_WARN(logger, "Init CrVmem refused");
        }
    }

    const int testCount = 100000;
    for (int i = 0; i < testCount; i++) {
        double action = randf64(re);
        if (action < 1.0) { // 新建虚拟地址空间
            auto new_pagetable = sv->create_pagetable();
            if (new_pagetable != 0) {
                goldModels[new_pagetable] = {};
                SPDLOG_LOGGER_DEBUG(logger, "CrVmem VMEM @ paddr 0x{:x}", new_pagetable);
            } else {
                SPDLOG_LOGGER_DEBUG(logger, "CrVmem refused");
            }
        } else if (action < 2.0) { // 销毁虚拟地址空间
            if (goldModels.empty()) continue;
            auto vmem_it = goldModels.begin();
            std::advance(vmem_it, std::rand() % goldModels.size());
            auto vmem = vmem_it->first;
            if (sv->destroy_pagetable(vmem) == 0) {
                SPDLOG_LOGGER_DEBUG(logger, "RmVmem VMEM @ paddr 0x{:x}", vmem);
                goldModels.erase(vmem);
            } else {
                SPDLOG_LOGGER_WARN(logger, "RmVmem refused");
                assert(0);
            }
        } else if (action < 10.0) { // 申请内存区域&写操作
            if (goldModels.empty()) continue;
            auto it = goldModels.begin();
            std::advance(it, std::rand() % goldModels.size());
            auto vmem = it->first;
            vaddr_t vaddr = (std::rand() % 1000) * PAGESIZE;
            size_t dataSize = 1 + std::rand() % 8192; // 数据大小
            vaddr = sv->mmap(vmem, vaddr, dataSize);
            if (vaddr == 0) {
                SPDLOG_LOGGER_DEBUG(logger, "WrData mmap refused");
            } else {
                std::vector<uint8_t> testData(dataSize);
                for (size_t j = 0; j < dataSize; j++) {
                    testData[j] = static_cast<uint8_t>(std::rand());
                }
                if (!mmu->memcpy(vmem, vaddr, testData.data(), dataSize)) {
                    SPDLOG_LOGGER_ERROR(logger, "WrData memcpy failed");
                    return -1;
                }
                goldModels[vmem][vaddr] = testData;
                SPDLOG_LOGGER_DEBUG(logger, "WrData VMEM @ vaddr 0x{:x}, size {}", vaddr, dataSize);
            }
        } else if (action < 18.0) { // 释放虚拟内存
            if (goldModels.empty()) continue;
            auto vmem_it = goldModels.begin();
            std::advance(vmem_it, std::rand() % goldModels.size());
            auto vmem = vmem_it->first;
            if (goldModels[vmem].empty()) {
                SPDLOG_LOGGER_DEBUG(logger, "RmData VMEM @ paddr 0x{:x} skipped as empty", vmem);
            } else {
                auto vdata_it = goldModels[vmem].begin();
                std::advance(vdata_it, std::rand() % goldModels[vmem].size());
                vaddr_t vaddr = vdata_it->first;
                size_t dataSize = goldModels[vmem][vaddr].size();
                if (sv->munmap(vmem, vaddr, dataSize) == 0) {
                    SPDLOG_LOGGER_DEBUG(
                        logger, "RmData VMEM @ vaddr 0x{:x}, size {}", vaddr, dataSize
                    );
                    goldModels[vmem].erase(vaddr);
                } else {
                    SPDLOG_LOGGER_WARN(
                        logger, "RmData VMEM @ vaddr 0x{:x}, size {} refused", vaddr, dataSize
                    );
                    assert(0);
                }
            }
        } else { // 读操作：随机选择一条记录验证数据
            if (goldModels.empty()) continue;
            auto vmem_it = goldModels.begin();
            std::advance(vmem_it, std::rand() % goldModels.size());
            auto vmem = vmem_it->first;
            if (goldModels[vmem].empty()) {
                SPDLOG_LOGGER_DEBUG(logger, "RdData VMEM @ paddr 0x{:x} skipped as empty", vmem);
            } else {
                auto vdata_it = goldModels[vmem].begin();
                std::advance(vdata_it, std::rand() % goldModels[vmem].size());
                auto vaddr = vdata_it->first;
                const auto &expectedData = vdata_it->second;
                std::vector<uint8_t> readData(expectedData.size(), 0);
                if (!mmu->memcpy(vmem, readData.data(), vaddr, expectedData.size())) {
                    SPDLOG_LOGGER_ERROR(logger, "RdData memcpy failed");
                    return -1;
                }
                if (readData == expectedData) {
                    SPDLOG_LOGGER_DEBUG(
                        logger, "RdData VMEM @ vaddr 0x{:x}, size {}, PASS", vaddr,
                        expectedData.size()
                    );
                } else {
                    SPDLOG_LOGGER_ERROR(
                        logger, "RdData VMEM @ vaddr 0x{:x}, size {}, FAIL", vaddr,
                        expectedData.size()
                    );
                    assert(0);
                }
            }
        }
    }

    // 销毁剩余的虚拟地址空间
    for (const auto &kv : goldModels) {
        sv->destroy_pagetable(kv.first);
    }
    assert(sv->get_vmem_usage() == 0);
    assert(sv->get_pmem_usage() == 0);
    SPDLOG_LOGGER_DEBUG(logger, "All test passed");
    return 0;
}

#include "sv32.hpp"
#include "sv39.hpp"

int main() {
    auto logger = spdlog::stdout_color_mt("main");
    int result39 = test<SV39_basic, SV39_supervisor>(logger);
    int result32 = test<SV32_basic, SV32_supervisor>(logger);

    if (result39 == 0 && result32 == 0) {
        SPDLOG_LOGGER_INFO(logger, "All test passed: SV39 and SV32");
        return 0;
    } else {
        return -1;
    }
}
