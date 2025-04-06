#include "sv32_supervisor.hpp"
#include "physical_mem.hpp"
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

SV32_supervisor::SV32_supervisor(
    std::shared_ptr<PhysicalMemoryInterface> pmem_, std::shared_ptr<spdlog::logger> logger_
)
    : SV32_basic(pmem_, logger_), buddy(pmem_->m_size / PAGESIZE, 11) {}

SV32_supervisor::pagetable_t SV32_supervisor::create_pagetable() {
    paddr_t ptroot = buddy.allocate(0);
    if (ptroot == 0) {
        return 0;
    }
    assert(ptroot % PAGESIZE == 0);
    assert(std::all_of(m_ptroots.begin(), m_ptroots.end(), [ptroot](auto &existing_ptroot) {
        return existing_ptroot != ptroot;
    }));
    m_ptroots.push_back(ptroot);
    if (pmem->fill(ptroot, 0, PAGESIZE)) {
        logger->error("SV32 failed to reset newly allocated pagetable to 0 at PMEM 0x{:x}", ptroot);
        assert(0);
        buddy.free(ptroot, 0);
        return 0;
    }
    return ptroot;
}

int SV32_supervisor::destroy_pagetable_one_level(const pagetable_t ptaddr, int level) {
    assert(ptaddr % PAGESIZE == 0);
    using PTE = BITRANGE::PTE;
    using VA = BITRANGE::VA;
    using PA = BITRANGE::PA;
    for (paddr_t pte_addr = ptaddr; pte_addr < ptaddr + PAGESIZE; pte_addr += sizeof(pte_t)) {
        pte_t pte;
        if (pmem->read(pte_addr, &pte, sizeof(pte_t))) {
            logger->error("SV32 failed to get PTE from PMEM 0x{:x}", pte_addr);
            assert(0);
            return -1;
        }
        if (bits_extract(pte, PTE::V) == 0) {
            continue; // non-valid pte, no need to free
        }
        if (bits_extract(pte, PTE::XWR)) { // leaf PTE
            if (level != 0) {
                logger->error("SV32 internal page-free error: large page not supported yet");
                assert(0);
                return -1;
            }
            paddr_t paddr = bits_set(bits_extract(pte, PTE::PPNFULL), PA::PPNFULL);
            buddy.free(paddr, 0);
            assert(m_vpage_usage > 0);
            m_vpage_usage--;
        } else { // pointer to next level pagetable
            if (level == 0) {
                logger->error(
                    "SV32 PTE error: point to non-exist next level pagetable "
                    "PAGE-FAULT, ptroot=0x{:x}, vaddr=0x{:x}",
                    ptaddr, 0
                );
                assert(0);
                return -1;
            }
            paddr_t next_ptaddr = bits_set(bits_extract(pte, PTE::PPNFULL), PA::PPNFULL);
            if (destroy_pagetable_one_level(next_ptaddr, level - 1)) {
                return -1;
            }
        }
    }
    // 释放本级页表占用的物理页
    buddy.free(ptaddr, 0);
    return 0;
}

int SV32_supervisor::destroy_pagetable(pagetable_t ptroot) {
    assert_ptroot(ptroot);
    int result = destroy_pagetable_one_level(ptroot, LEVELS - 1);
    if (result == 0) {
        m_ptroots.erase(std::remove(m_ptroots.begin(), m_ptroots.end(), ptroot), m_ptroots.end());
    }
    return result;
}

SV32_supervisor::vaddr_t SV32_supervisor::mmap(
    const pagetable_t ptroot, vaddr_t vaddr, const size_t size
) {
    if (size == 0) {
        return 0;
    }
    assert_ptroot(ptroot);
    vaddr = vaddr - vaddr % PAGESIZE;
    vaddr = (vaddr == 0) ? 0x91000000 : vaddr;
    const int num_page = (size + PAGESIZE - 1) / PAGESIZE;
    // todo: how to determine we cannot find idle vaddr?
    //       currently only search idle vaddr for 4096 times, if not found, alloc failed
    for (int i = 0; i < 4096; i++) {
        bool idle_vaddr_found = true; // default
        for (size_t pgcnt = 0; pgcnt < num_page; pgcnt++) {
            if (translate(ptroot, vaddr + pgcnt * PAGESIZE) != 0) {
                idle_vaddr_found = false;
                break;
            }
        }
        if (idle_vaddr_found) {
            break;
        }
        if (i == 4095) {
            return 0;
        }
        vaddr += PAGESIZE;
    }
    // idle vaddr found
    for (size_t pgcnt = 0; pgcnt < num_page; pgcnt++) {
        if (alloc_one_page(ptroot, vaddr + pgcnt * PAGESIZE)) {
            // failed to alloc page. rollback needed: free all previous allocated pages
            for (size_t pgcnt_free = 0; pgcnt_free < pgcnt; pgcnt_free++) {
                assert(free_one_page(ptroot, vaddr + pgcnt_free * PAGESIZE) == 0);
            }
            return 0;
        }
    }
    return vaddr;
}

int SV32_supervisor::munmap(const pagetable_t ptroot, vaddr_t vaddr, size_t size) {
    assert_ptroot(ptroot);
    assert(vaddr % PAGESIZE == 0);
    if (size == 0) {
        return -1;
    }
    size_t num_page = (size + PAGESIZE - 1) / PAGESIZE;

    for (size_t pgcnt = 0; pgcnt < num_page; pgcnt++) {
        if (free_one_page(ptroot, vaddr + pgcnt * PAGESIZE)) {
            return -1;
        }
    }
    return 0;
}

int SV32_supervisor::alloc_one_page(const pagetable_t ptroot, const vaddr_t vaddr) {
    assert_ptroot(ptroot);
    assert(vaddr % PAGESIZE == 0);
    assert(!translate(ptroot, vaddr));
    using PTE = BITRANGE::PTE;
    using VA = BITRANGE::VA;
    using PA = BITRANGE::PA;
    paddr_t paddr = 0;

    std::vector<paddr_t> allocated_pages;
    std::vector<std::pair<paddr_t, pte_t>> commit_ptes;

    paddr_t ptaddr = ptroot;
    int level;
    pte_t pte = 0;
    paddr_t pte_addr = 0;
    for (level = LEVELS - 1; level >= 0; level--) {
        pte_addr = ptaddr + bits_extract(vaddr, VA::VPN[level]) * sizeof(pte_t);
        if (pmem->read(pte_addr, &pte, sizeof(pte_t))) {
            logger->error(
                "SV32 failed to get PTE from PMEM at 0x{:x}, "
                "ptroot=0x{:x}, vaddr=0x{:x}",
                pte_addr, ptroot, vaddr
            );
            assert(0); // it's a incorrect pagetable, please check where destroy it
            return -1;
        }
        if (bits_extract(pte, PTE::V) == 0) {
            break;
        }
        if (bits_extract(pte, PTE::R) == 0 && bits_extract(pte, PTE::W) == 1) {
            logger->error(
                "SV32 PTE error: R=0 && W=1 PAGE-FAULT, ptroot=0x{:x}, vaddr=0x{:x}", ptroot, vaddr
            );
            assert(0); // todo: how to deal with pagefault exception?
            return 0;
        }
        // Already known pte.v == 1
        if (bits_extract(pte, PTE::R) || bits_extract(pte, PTE::X)) {
            // Leaf PTE found
            logger->error(
                "SV32 internal alloc error: alloc at existing vaddr 0x{:x}, "
                "this problem should be already solved by caller(mmap)",
                vaddr
            );
            assert(0);
        } else {              // Next level PTE found
            if (level == 0) { // already reach final level, next level does not exist
                logger->error(
                    "SV32 PTE error: point to non-exist next level pagetable "
                    "PAGE-FAULT, ptroot=0x{:x}, vaddr=0x{:x}",
                    ptroot, vaddr
                );
                assert(0);
                return 0;
            }
            ptaddr = bits_set(bits_extract(pte, PTE::PPNFULL), PA::PPNFULL);
            continue;
        }
    }
    while (level > 0) { // need to create new 4kB pagetable
        ptaddr = buddy.allocate(0);
        if (ptaddr == 0) {
            goto RET_ERR;
        }
        assert(ptaddr % PAGESIZE == 0);
        allocated_pages.push_back(ptaddr);
        pte = bits_set(bits_extract(ptaddr, PA::PPNFULL), PTE::PPNFULL, 0);
        pte = bits_set(1, PTE::V, pte);
        // pte.X/W/R = 0, pointer to next level pagetable
        commit_ptes.push_back({pte_addr, pte});
        // loop step
        level--;
        pte_addr = ptaddr + bits_extract(vaddr, VA::VPN[level]) * sizeof(pte_t);
    }
    assert(level == 0);

    paddr = buddy.allocate(0);
    if (paddr == 0) {
        goto RET_ERR;
    }
    assert(paddr % PAGESIZE == 0);
    // no need to allocated_pages.push_back(paddr), we are already success
    pte = bits_set(bits_extract(paddr, PA::PPNFULL), PTE::PPNFULL, 0);
    pte = bits_set(1, PTE::V, pte);
    // todo: currently, accessibility is not checked strictly
    pte = bits_set(1, PTE::R, pte);
    pte = bits_set(1, PTE::X, pte);
    pte = bits_set(1, PTE::W, pte);
    commit_ptes.push_back({pte_addr, pte});

    // success to alloc one page, commit all changes now
    for (auto &page : allocated_pages) {
        if (pmem->fill(page, 0, PAGESIZE)) {
            logger->error(
                "SV32 failed to reset newly allocated pagetable to 0 at PMEM 0x{:x}, "
                "ptroot=0x{:x}, vaddr=0x{:x}",
                page, ptroot, vaddr
            );
            assert(0);
            goto RET_ERR;
        }
    }
    for (auto &p : commit_ptes) {
        if (pmem->write(p.first, &p.second, sizeof(pte_t))) {
            logger->error(
                "SV32 failed to write PTE to PMEM at 0x{:x}, "
                "ptroot=0x{:x}, vaddr=0x{:x}",
                p.first, ptroot, vaddr
            );
            assert(0);
            goto RET_ERR;
        }
    }
    m_vpage_usage++;
    return 0;

RET_ERR:
    for (auto &p : allocated_pages) {
        buddy.free(p, 0);
    }
    return -1;
}

int SV32_supervisor::free_one_page(pagetable_t ptroot, vaddr_t vaddr) {
    assert_ptroot(ptroot);
    assert(vaddr % PAGESIZE == 0);
    assert(translate(ptroot, vaddr));
    using PTE = BITRANGE::PTE;
    using VA = BITRANGE::VA;
    using PA = BITRANGE::PA;

    std::vector<paddr_t> pagetables;
    std::vector<paddr_t> pte_addrs;
    paddr_t ptaddr = ptroot; // the selected-level pagetable base addr
    for (int level = LEVELS - 1; level >= 0; level--) {
        paddr_t pte_addr = ptaddr + bits_extract(vaddr, VA::VPN[level]) * sizeof(pte_t);
        pte_t pte;
        if (pmem->read(pte_addr, &pte, sizeof(pte_t))) {
            logger->error(
                "SV32 failed to get PTE from PMEM at 0x{:x}, "
                "ptroot=0x{:x}, vaddr=0x{:x}",
                pte_addr, ptroot, vaddr
            );
            assert(0);
            return -1;
        }
        if (bits_extract(pte, PTE::V) == 0) {
            logger->error(
                "SV32 PTE.V==0 page-missing PAGE-FAULT during internal page-free, PTE at "
                "PMEM 0x{:x}, ptroot=0x{:x}, vaddr=0x{:x}",
                pte_addr, ptroot, vaddr
            );
            assert(0);
            return -1;
        }
        if (bits_extract(pte, PTE::R) == 0 && bits_extract(pte, PTE::W) == 1) {
            logger->error(
                "SV32 PTE error: R=0 && W=1 PAGE-FAULT, ptroot=0x{:x}, vaddr=0x{:x}", ptroot, vaddr
            );
            assert(0);
            return -1;
        }
        // Already known pte.v == 1
        if (bits_extract(pte, PTE::R) || bits_extract(pte, PTE::X)) {
            // Leaf PTE found
            if (level != 0) { // for super-page (level != 0)
                logger->error(
                    "SV32 page free do not support superpage yet, ptroot=0x{:x}, vaddr=0x{:x}",
                    ptroot, vaddr
                );
            }
            paddr_t paddr = bits_set(bits_extract(pte, PTE::PPNFULL), PA::PPNFULL);
            assert(paddr != 0);
            buddy.free(paddr, 0);
            pte = 0;
            if (pmem->write(pte_addr, &pte, sizeof(pte_t))) {
                logger->error(
                    "SV32 failed to write PTE to PMEM at 0x{:x}, "
                    "ptroot=0x{:x}, vaddr=0x{:x}",
                    pte_addr, ptroot, vaddr
                );
                assert(0);
                return -1;
            }
            // TODO: maybe we need to check if the pagetables need to be freed
            //       but currently, we do not free pagetables until destroy_pagetable
            assert(m_vpage_usage > 0);
            m_vpage_usage--;
            return 0;
        } else { // Next level PTE found
            if (level == 0) {
                logger->error(
                    "SV32 PTE error: point to non-exist next level pagetable PAGE-FAULT, "
                    "ptroot=0x{:x}, vaddr=0x{:x}",
                    ptroot, vaddr
                );
                assert(0);
                return -1;
            }
            ptaddr = bits_set(bits_extract(pte, PTE::PPNFULL), PA::PPNFULL);
            continue;
        }
    }
    assert(0); // should already return in the loop
    return -1;
}

void SV32_supervisor::assert_ptroot(pagetable_t ptroot) {
    assert(ptroot % PAGESIZE == 0);
    assert(std::find(m_ptroots.begin(), m_ptroots.end(), ptroot) != m_ptroots.end());
}
