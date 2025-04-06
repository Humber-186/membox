#pragma once

#include <cstdint>
#include "sv_basic.hpp"
#include "sv_supervisor.hpp"

struct SV32_Trait {
    static constexpr int LEVELS = 2;
    using vaddr_t = uint32_t;
    using pte_t = uint32_t;

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
};

class SV32_basic : public SV_basic<SV32_Trait> {
public:
    SV32_basic(
        std::shared_ptr<PhysicalMemoryInterface> pmem,
        std::shared_ptr<spdlog::logger> logger = nullptr
    )
        : SV_basic<SV32_Trait>(pmem, logger) {}
};

class SV32_supervisor : public SV_supervisor<SV32_Trait> {
public:
    SV32_supervisor(
        std::shared_ptr<PhysicalMemoryInterface> pmem,
        std::shared_ptr<spdlog::logger> logger = nullptr
    )
        : SV_supervisor<SV32_Trait>(pmem, logger) {}
};
