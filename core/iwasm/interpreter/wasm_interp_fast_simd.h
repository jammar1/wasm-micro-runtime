#ifndef WASM_INTERP_FAST_SIMD_H
#define WASM_INTERP_FAST_SIMD_H

#include "wasm_runtime.h"

struct WASMModuleInstance;
struct WASMInterpFrame;

uint8 *
wasm_simd_handle_op(WASMModuleInstance *module_inst, uint8 *frame_ip,
                    uint32 *frame_lp, uint8 *maddr, uint32 *p_addr_ret,
                    uint8 opcode, uint64 linear_mem_size
#if !defined(OS_ENABLE_HW_BOUND_CHECK) \
    || WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS == 0
                    ,
                    bool disable_bounds_checks
#endif
);
#endif /* WASM_INTERP_FAST_SIMD_H */