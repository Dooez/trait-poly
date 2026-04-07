#pragma once

#ifndef TRP_GODBOLT
#include "trp_concepts.hpp"
#endif
namespace trp::detail {
struct empty_0 {};

template<typename Arc = empty_0>
struct ctrl_header {
    using dtor_ptr = void (*)(ctrl_header*);
    dtor_ptr                  destructor_ptr_{};
    [[no_unique_address]] Arc counter;
};

template<typename Impl, typename Allocator, typename Arc = empty_0>
struct ctrl_block : public ctrl_header<Arc> {
    ctrl_block() = delete;

    template<typename... Args>
    ctrl_block(std::byte* memstart, uZ n, Allocator&& allocator, Args&&... args)
    : ctrl_header<Arc>{.destructor_ptr_ = &destroy}
    , impl_(std::forward<Args>(args)...)
    , memstart_(memstart)
    , n_(n)
    , allocator_(std::move(allocator)){};

    Impl       impl_;
    std::byte* memstart_{};
    uZ         n_{};

    [[no_unique_address]] Allocator allocator_;

    static void destroy(ctrl_header<Arc>* ctrl_ptr) {
        auto& ctrl      = *static_cast<ctrl_block*>(ctrl_ptr);
        auto  allocator = std::move(ctrl.allocator_);
        auto  memstart  = ctrl.memstart_;
        auto  n         = ctrl.n_;
        ctrl.~ctrl_block();
        std::allocator_traits<Allocator>::deallocate(allocator, memstart, n);
    }
};
}    // namespace trp::detail
