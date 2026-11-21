#pragma once
#include <atomic>

inline bool TryBeginUpdateCheck(std::atomic_bool& checking) noexcept {
    bool expected=false;
    return checking.compare_exchange_strong(expected,true,
        std::memory_order_acq_rel,std::memory_order_acquire);
}

inline void FinishUpdateCheck(std::atomic_bool& checking) noexcept {
    checking.store(false,std::memory_order_release);
}
