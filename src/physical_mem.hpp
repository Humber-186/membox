#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <spdlog/logger.h>

class PhysicalMemory {
public:
    using paddr_t = uint64_t;
    const uint64_t m_size;
    const paddr_t m_addr_floor = 4096;

public:
    PhysicalMemory(uint64_t size = (1ull << 30), std::shared_ptr<spdlog::logger> logger = nullptr)
        : m_size(size), m_logger(logger) {
        m_mem = new (std::align_val_t(4096)) uint8_t[m_size];
    }
    ~PhysicalMemory() { operator delete[](m_mem, std::align_val_t(4096)); }

    int write(paddr_t addr, const void *src, size_t size) {
        if (addr_check(addr, size)) {
            return -1;
        }
        memcpy(m_mem + addr, src, size);
        return 0;
    }
    int write(paddr_t addr, const void *src, const bool mask[], size_t size) {
        if (addr_check(addr, size) != 0) {
            return -1;
        }
        for (size_t i = 0; i < size; i++) {
            if (mask[i]) {
                m_mem[addr + i] = ((uint8_t *)src)[i];
            }
        }
        return 0;
    }
    int fill(paddr_t addr, uint8_t value, size_t size) {
        if (addr_check(addr, size) != 0) {
            return -1;
        }
        memset(m_mem + addr, value, size);
        return 0;
    }
    int read(paddr_t addr, void *dst, size_t size) {
        if (addr_check(addr, size) != 0) {
            return -1;
        }
        memcpy(dst, m_mem + addr, size);
        return 0;
    }
    int alloc(paddr_t addr, size_t size) {
        if (addr_check(addr, size) != 0) {
            return -1;
        }
        return 0;
    }
    int free(paddr_t addr, size_t size) {
        if (addr_check(addr, size) != 0) {
            return -1;
        }
        return 0;
    }

private:
    uint8_t *m_mem;
    std::shared_ptr<spdlog::logger> m_logger;
    int addr_check(paddr_t addr, size_t size = 0) {
        if (addr < m_addr_floor || addr + size > m_size) {
            if (size == 0)
                m_logger->error("PMEM addr out of range: 0x{:x}", addr);
            else
                m_logger->error("PMEM addr out of range: 0x{:x} + {}", addr, size);
            return -1;
        }
        return 0;
    }
};
