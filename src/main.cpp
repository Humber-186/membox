#include "physical_mem.hpp"
#include "sv39_basic.hpp"
#include "sv39_supervisor.hpp"
#include <cmath>
#include <fmt/format.h>
#include <memory>
#include <random>
#include <spdlog/sinks/stdout_color_sinks.h>
// 新增的头文件
#include <cstdlib>
#include <ctime>
#include <map>
#include <vector>

int main(int argc, char **argv) {
    auto logger = spdlog::stdout_color_mt("main");
    auto pmem = std::make_shared<PhysicalMemory>((1ull << 32), logger);
    auto sv39 = std::make_shared<SV39_supervisor>(pmem, logger);
    auto mmu = std::make_shared<SV39_basic>(pmem, logger);

    //
    // --- 测试基本功能 ---
    //

    SV39_basic::pagetable_t vmem1 = sv39->create_pagetable();
    vaddr_t vaddr1 = 0x1000;
    char data[] = "Hello, World!";
    sv39->mmap(vmem1, vaddr1, sizeof(data));
    mmu->memcpy(vmem1, vaddr1, data, sizeof(data));

    paddr_t paddr1 = mmu->translate(vmem1, vaddr1);
    char data_read_out[128];
    mmu->memcpy(vmem1, data_read_out, vaddr1, sizeof(data));

    logger->info("vaddr1=0x{:x}, paddr1=0x{:x}, data_read_out={}", vaddr1, paddr1, data_read_out);
    sv39->munmap(vmem1, vaddr1, sizeof(data));
    sv39->destroy_pagetable(vmem1);

    //
    // --- 随机测试 ---
    //

    std::srand(std::time(nullptr));
    std::uniform_real_distribution<double> randf64(0, 100);
    std::default_random_engine re;

    // 删除原有的pagetables和goldModels定义，改为：
    std::map<SV39_basic::pagetable_t, std::map<vaddr_t, std::vector<uint8_t>>> goldModels;

    // 初始化一些虚拟地址空间，每个空间中写入5段随机数据
    for (int i = 0; i < 5; i++) {
        auto vmem = sv39->create_pagetable();
        if (vmem != 0) {
            goldModels[vmem] = {}; // 新增虚拟地址空间与其gold model映射
            for (int j = 0; j < 5; j++) {
                vaddr_t vaddr = (std::rand() % 1000) * SV39_basic::PAGESIZE; // 页对齐地址
                size_t dataSize = 1 + std::rand() % 8192;                    // 数据大小
                vaddr = sv39->mmap(vmem, vaddr, dataSize);
                if (vaddr == 0) {
                    logger->warn("Init WrData mmap refused");
                    continue;
                }
                // 生成随机数据
                std::vector<uint8_t> testData(dataSize);
                for (size_t k = 0; k < dataSize; k++) {
                    testData[k] = static_cast<uint8_t>(std::rand());
                }
                // 写数据到虚拟内存
                mmu->memcpy(vmem, vaddr, testData.data(), dataSize);
                // 更新gold model
                goldModels[vmem][vaddr] = testData;
                logger->info("Init WrData VMEM {:x}, vaddr 0x{:x}, size {}", vmem, vaddr, dataSize);
            }
        } else {
            logger->warn("Init CrVmem refused");
        }
    }

    const int testCount = 50000;
    for (int i = 0; i < testCount; i++) {
        double action = randf64(re);
        if (action < 1.0) { // 新建虚拟地址空间
            auto new_pagetable = sv39->create_pagetable();
            if (new_pagetable != 0) {
                goldModels[new_pagetable] = {};
                logger->info("CrVmem VMEM @ paddr 0x{:x}", new_pagetable);
            } else {
                logger->info("CrVmem refused");
            }
        } else if (action < 2.0) { // 销毁虚拟地址空间
            if (goldModels.empty())
                continue;
            auto vmem_it = goldModels.begin();
            std::advance(vmem_it, std::rand() % goldModels.size());
            auto vmem = vmem_it->first;
            if (sv39->destroy_pagetable(vmem) == 0) {
                logger->info("RmVmem VMEM @ paddr 0x{:x}", vmem);
                goldModels.erase(vmem);
            } else {
                logger->error("RmVmem refused");
                assert(0);
            }
        } else if (action < 10.0) { // 申请内存区域&写操作
            if (goldModels.empty())
                continue;
            auto it = goldModels.begin();
            std::advance(it, std::rand() % goldModels.size());
            auto vmem = it->first;
            vaddr_t vaddr = (std::rand() % 1000) * SV39_basic::PAGESIZE;
            size_t dataSize = 1 + std::rand() % 8192; // 数据大小
            vaddr = sv39->mmap(vmem, vaddr, dataSize);
            if (vaddr == 0) {
                logger->info("WrData mmap refused");
            } else {
                std::vector<uint8_t> testData(dataSize);
                for (size_t j = 0; j < dataSize; j++) {
                    testData[j] = static_cast<uint8_t>(std::rand());
                }
                mmu->memcpy(vmem, vaddr, testData.data(), dataSize);
                goldModels[vmem][vaddr] = testData;
                logger->info("WrData VMEM @ vaddr 0x{:x}, size {}", vaddr, dataSize);
            }
        } else if (action < 18.0) { // 释放虚拟内存
            if (goldModels.empty())
                continue;
            auto vmem_it = goldModels.begin();
            std::advance(vmem_it, std::rand() % goldModels.size());
            auto vmem = vmem_it->first;
            if (goldModels[vmem].empty()) {
                logger->info("RmData VMEM @ paddr 0x{:x} skipped as empty", vmem);
            } else {
                auto vdata_it = goldModels[vmem].begin();
                std::advance(vdata_it, std::rand() % goldModels[vmem].size());
                vaddr_t vaddr = vdata_it->first;
                size_t dataSize = goldModels[vmem][vaddr].size();
                if (sv39->munmap(vmem, vaddr, dataSize) == 0) {
                    logger->info("RmData VMEM @ vaddr 0x{:x}, size {}", vaddr, dataSize);
                    goldModels[vmem].erase(vaddr);
                } else {
                    logger->error("RmData VMEM @ vaddr 0x{:x}, size {} refused", vaddr, dataSize);
                    assert(0);
                }
            }
        } else { // 读操作：随机选择一条记录验证数据
            if (goldModels.empty())
                continue;
            auto vmem_it = goldModels.begin();
            std::advance(vmem_it, std::rand() % goldModels.size());
            auto vmem = vmem_it->first;
            if (goldModels[vmem].empty()) {
                logger->info("RdData VMEM @ paddr 0x{:x} skipped as empty", vmem);
            } else {
                auto vdata_it = goldModels[vmem].begin();
                std::advance(vdata_it, std::rand() % goldModels[vmem].size());
                auto vaddr = vdata_it->first;
                const auto &expectedData = vdata_it->second;
                std::vector<uint8_t> readData(expectedData.size(), 0);
                mmu->memcpy(vmem, readData.data(), vaddr, expectedData.size());
                if (readData == expectedData) {
                    logger->info("RdData VMEM @ vaddr 0x{:x}, size {}, PASS", vaddr,
                                 expectedData.size());
                } else {
                    logger->error("RdData VMEM @ vaddr 0x{:x}, size {}, FAIL", vaddr,
                                  expectedData.size());
                    assert(0);
                }
            }
        }
    }

    // 销毁剩余的虚拟地址空间
    for (const auto &kv : goldModels) {
        sv39->destroy_pagetable(kv.first);
    }
    assert(sv39->get_vmem_usage() == 0);
    assert(sv39->get_pmem_usage() == 0);
    logger->info("All test passed");
    return 0;
}
