#include "sv_basic.hpp"

#include "physical_mem.hpp"
#include <cassert>
#include <spdlog/spdlog.h>

template <typename Trait>
SV_basic<Trait>::SV_basic(
    std::shared_ptr<PhysicalMemoryInterface> pmem, std::shared_ptr<spdlog::logger> logger_
)
    : pmem(pmem) {
    this->logger = logger_ ? logger_ : spdlog::default_logger();
}

template <typename Trait>
uint64_t SV_basic<Trait>::bits_extract(uint64_t data, std::pair<uint8_t, uint8_t> range) {
    assert(range.first >= range.second);
    assert(range.first < 64);
    assert(range.second < 64);
    uint8_t width = range.first - range.second + 1;
    uint64_t mask = (width == 64) ? ~0ull : (1ull << width) - 1;
    return (data >> range.second) & mask;
}

template <typename Trait>
uint64_t SV_basic<Trait>::bits_set(
    uint64_t value, std::pair<uint8_t, uint8_t> range, uint64_t data
) {
    assert(range.first >= range.second);
    assert(range.first < 64);
    assert(range.second < 64);
    uint8_t width = range.first - range.second + 1;
    uint64_t mask = (width == 64) ? ~0ull : (1ull << width) - 1;
    assert((value & ~mask) == 0);
    return (data & ~(mask << range.second)) | ((value & mask) << range.second);
}

template <typename Trait>
typename SV_basic<Trait>::paddr_t SV_basic<Trait>::translate(
    const paddr_t ptroot, const vaddr_t vaddr
) const {
    assert(ptroot % PAGESIZE == 0);
    using PTE = typename BITRANGE::PTE;
    using VA = typename BITRANGE::VA;
    using PA = typename BITRANGE::PA;
    paddr_t ptaddr = ptroot; // the selected-level pagetable base addr
    for (int level = LEVELS - 1; level >= 0; level--) {
        paddr_t pte_addr = ptaddr + bits_extract(vaddr, VA::VPN[level]) * sizeof(pte_t);
        pte_t pte;
        if (pmem->read(pte_addr, &pte, sizeof(pte_t))) {
            SPDLOG_LOGGER_ERROR(
                logger, "SV failed to get PTE from PMEM 0x{:x}, ptroot=0x{:x}, vaddr=0x{:x}",
                pte_addr, ptroot, vaddr
            );
            assert(0);
            return 0;
        }
        if (bits_extract(pte, PTE::V) == 0) {
            // If this is hardware MMU, we should raise PAGE-FAULT exception here
            // If this is software vmem supervisor, this is normal (no paddr assigned to this vaddr)
            return 0;
        }
        if (bits_extract(pte, PTE::R) == 0 && bits_extract(pte, PTE::W) == 1) {
            SPDLOG_LOGGER_ERROR(
                logger, "SV PTE error: R=0 && W=1 PAGE-FAULT, ptroot=0x{:x}, vaddr=0x{:x}", ptroot,
                vaddr
            );
            assert(0); // todo: how to deal with pagefault exception?
            return 0;
        }
        // Already known pte.v == 1
        if (bits_extract(pte, PTE::R) || bits_extract(pte, PTE::X)) {
            // Leaf PTE found
            // TODO: need to check accessibility (PTE.R/W/X/U bits)
            // TODO: need to deal with PTE.A/D
            paddr_t paddr = bits_set(bits_extract(vaddr, VA::PAGEOFFSET), PA::PAGEOFFSET, 0ull);
            for (int i = 0; i < level; i++) { // for super-page (level != 0)
                // lower-level PTE.PPN should be 0
                if (bits_extract(pte, PTE::PPN[level]) != 0ull) {
                    SPDLOG_LOGGER_ERROR(
                        logger,
                        "SV PTE error: superpage PTE.PPN[{}]!=0 PAGE-FAULT, "
                        "ptroot=0x{:x}, vaddr=0x{:x}",
                        level, ptroot, vaddr
                    );
                }
                // lower-level PA.PPN[i] = VA.VPN[i]
                paddr = bits_set(bits_extract(vaddr, VA::VPN[i]), PA::PPN[i], paddr);
            }
            for (int i = LEVELS - 1; i >= level; i--) {
                paddr = bits_set(bits_extract(pte, PTE::PPN[i]), PA::PPN[i], paddr);
            }
            assert(paddr != 0);
            return paddr;
        } else {              // Next level PTE found
            if (level == 0) { // already reach final level, next level does not exist
                SPDLOG_LOGGER_ERROR(
                    logger,
                    "SV PTE error: point to non-exist next level pagetable PAGE-FAULT, "
                    "ptroot=0x{:x}, vaddr=0x{:x}",
                    ptroot, vaddr
                );
                assert(0);
                return 0;
            }
            ptaddr = bits_set(bits_extract(pte, PTE::PPNFULL), PA::PPNFULL);
            continue;
        }
    }
    // should always return in the loop
    assert(false);
    return -1;
}

template <typename Trait>
typename SV_basic<Trait>::vaddr_t SV_basic<Trait>::memcpy(
    pagetable_t pagetable_root, vaddr_t dst, const void *src_, size_t size
) const {
    const uint8_t *src = static_cast<const uint8_t *>(src_);
    size_t offset = 0;
    while (offset < size) {
        vaddr_t cur_vaddr = dst + offset;
        size_t page_offset = cur_vaddr % PAGESIZE;
        size_t chunk = std::min(size - offset, static_cast<size_t>(PAGESIZE - page_offset));
        paddr_t cur_paddr = translate(pagetable_root, cur_vaddr);
        if (cur_paddr == 0) {
            SPDLOG_LOGGER_ERROR(
                logger, "SV memcpy(write): failed to translate vaddr=0x{:x}, ptroot=0x{:x}",
                cur_vaddr, pagetable_root
            );
            return 0;
        }
        if (pmem->write(cur_paddr, src + offset, chunk)) {
            SPDLOG_LOGGER_ERROR(
                logger, "SV memcpy(write): failed to write physical memory at 0x{:x}", cur_paddr
            );
            return 0;
        }
        offset += chunk;
    }
    return dst;
}

template <typename Trait>
void *SV_basic<Trait>::memcpy(pagetable_t ptroot, void *dst_, vaddr_t src, size_t size) const {
    uint8_t *dst = static_cast<uint8_t *>(dst_);
    size_t offset = 0;
    while (offset < size) {
        vaddr_t cur_vaddr = src + offset;
        size_t page_offset = cur_vaddr % PAGESIZE;
        size_t chunk = std::min(size - offset, static_cast<size_t>(PAGESIZE - page_offset));
        paddr_t cur_paddr = translate(ptroot, cur_vaddr);
        if (cur_paddr == 0) {
            SPDLOG_LOGGER_ERROR(
                logger, "SV memcpy(read): failed to translate vaddr=0x{:x}, ptroot=0x{:x}",
                cur_vaddr, ptroot
            );
            return nullptr;
        }
        if (pmem->read(cur_paddr, dst + offset, chunk)) {
            SPDLOG_LOGGER_ERROR(logger, "SV memcpy(read): failed to read PMEM 0x{:x}", cur_paddr);
            return nullptr;
        }
        offset += chunk;
    }
    return dst_;
}

#include "sv32.hpp"
template class SV_basic<SV32_Trait>;

#include "sv39.hpp"
template class SV_basic<SV39_Trait>;
