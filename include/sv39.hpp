#pragma once

#include "sv_basic.hpp"
#include "sv_supervisor.hpp"
#include <cstdint>

struct SV39_Trait {
    static constexpr int LEVELS = 3;
    using vaddr_t = uint64_t;
    using pte_t = uint64_t;

    struct BITRANGE {
        struct VA {
            static constexpr std::pair<uint8_t, uint8_t> PAGEOFFSET = {11, 00};
            static constexpr std::pair<uint8_t, uint8_t> VPN0 = {20, 12};
            static constexpr std::pair<uint8_t, uint8_t> VPN1 = {29, 21};
            static constexpr std::pair<uint8_t, uint8_t> VPN2 = {38, 30};
            static constexpr std::pair<uint8_t, uint8_t> VPN[LEVELS] = {VPN0, VPN1, VPN2};
        };
        struct PA {
            static constexpr std::pair<uint8_t, uint8_t> PAGEOFFSET = {11, 00};
            static constexpr std::pair<uint8_t, uint8_t> PPNFULL = {55, 12};
            static constexpr std::pair<uint8_t, uint8_t> PPN0 = {20, 12};
            static constexpr std::pair<uint8_t, uint8_t> PPN1 = {29, 21};
            static constexpr std::pair<uint8_t, uint8_t> PPN2 = {55, 30};
            static constexpr std::pair<uint8_t, uint8_t> PPN[LEVELS] = {PPN0, PPN1, PPN2};
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
            static constexpr std::pair<uint8_t, uint8_t> PPNFULL = {53, 10};
            static constexpr std::pair<uint8_t, uint8_t> PPN0 = {18, 10};
            static constexpr std::pair<uint8_t, uint8_t> PPN1 = {27, 19};
            static constexpr std::pair<uint8_t, uint8_t> PPN2 = {53, 28};
            static constexpr std::pair<uint8_t, uint8_t> PPN[LEVELS] = {PPN0, PPN1, PPN2};
            static constexpr std::pair<uint8_t, uint8_t> RESERVED = {60, 54};
            static constexpr std::pair<uint8_t, uint8_t> PBMT = {62, 61};
            static constexpr std::pair<uint8_t, uint8_t> N = {63, 63};
        };
    };
};

class SV39_basic : public SV_basic<SV39_Trait> {
public:
    SV39_basic(
        std::shared_ptr<PhysicalMemoryInterface> pmem,
        std::shared_ptr<spdlog::logger> logger = nullptr
    )
        : SV_basic<SV39_Trait>(pmem, logger) {}
};

class SV39_supervisor : public SV_supervisor<SV39_Trait> {
public:
    SV39_supervisor(
        std::shared_ptr<PhysicalMemoryInterface> pmem,
        std::shared_ptr<spdlog::logger> logger = nullptr
    )
        : SV_supervisor<SV39_Trait>(pmem, logger) {}
};
