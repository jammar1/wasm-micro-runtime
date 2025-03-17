#include "wasm_interp_fast_simd.h"
#include "wasm_opcode.h"
#include "wasm_interp_fast_macros.h"

#if WASM_ENABLE_SIMDE != 0
#include "simde/wasm/simd128.h"

#if WASM_ENABLE_SIMDE != 0
#define SIMD_V128_TO_SIMDE_V128(v)                                      \
    ({                                                                  \
        bh_assert(sizeof(V128) == sizeof(simde_v128_t));                \
        simde_v128_t result;                                            \
        bh_memcpy_s(&result, sizeof(simde_v128_t), &(v), sizeof(V128)); \
        result;                                                         \
    })

#define SIMDE_V128_TO_SIMD_V128(sv, v)                                \
    do {                                                              \
        bh_assert(sizeof(V128) == sizeof(simde_v128_t));              \
        bh_memcpy_s(&(v), sizeof(V128), &(sv), sizeof(simde_v128_t)); \
    } while (0)
    
    uint8 *
    wasm_simd_handle_op(WASMModuleInstance *module_inst, uint8 *frame_ip,
                        uint32 *frame_lp, uint8 *maddr, uint32 *p_addr_ret,
                        uint8 opcode, uint64 linear_mem_size
    #if !defined(OS_ENABLE_HW_BOUND_CHECK) \
        || WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS == 0
                        ,
                        bool disable_bounds_checks
    #endif
    )
    {
        WASMMemoryInstance *memory = wasm_get_default_memory(module_inst);
                switch (opcode) {
                    /* Memory */
                    case SIMD_v128_load:
                    {
                        uint32 offset, addr;
                        offset = read_uint32(frame_ip);
                        addr = POP_I32();
                        *p_addr_ret = GET_OFFSET();
                        CHECK_MEMORY_OVERFLOW(16);
                        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, LOAD_V128(maddr));
                        break;
                    }
#define SIMD_LOAD_OP(simde_func)                       \
    do {                                               \
        uint32 offset, addr;                           \
        offset = read_uint32(frame_ip);                \
        addr = POP_I32();                              \
        *p_addr_ret = GET_OFFSET();                       \
        CHECK_MEMORY_OVERFLOW(8);                      \
                                                       \
        simde_v128_t simde_result = simde_func(maddr); \
                                                       \
        V128 result;                                   \
        SIMDE_V128_TO_SIMD_V128(simde_result, result); \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result); \
                                                       \
    } while (0)
                    case SIMD_v128_load8x8_s:
                    {
                        SIMD_LOAD_OP(simde_wasm_i16x8_load8x8);
                        break;
                    }
                    case SIMD_v128_load8x8_u:
                    {
                        SIMD_LOAD_OP(simde_wasm_u16x8_load8x8);
                        break;
                    }
                    case SIMD_v128_load16x4_s:
                    {
                        SIMD_LOAD_OP(simde_wasm_i32x4_load16x4);
                        break;
                    }
                    case SIMD_v128_load16x4_u:
                    {
                        SIMD_LOAD_OP(simde_wasm_u32x4_load16x4);
                        break;
                    }
                    case SIMD_v128_load32x2_s:
                    {
                        SIMD_LOAD_OP(simde_wasm_i64x2_load32x2);
                        break;
                    }
                    case SIMD_v128_load32x2_u:
                    {
                        SIMD_LOAD_OP(simde_wasm_u64x2_load32x2);
                        break;
                    }
#define SIMD_LOAD_SPLAT_OP(simde_func, width)          \
    do {                                               \
        uint32 offset, addr;                           \
        offset = read_uint32(frame_ip);                \
        addr = POP_I32();                              \
        *p_addr_ret = GET_OFFSET();                       \
        CHECK_MEMORY_OVERFLOW(width / 8);              \
                                                       \
        simde_v128_t simde_result = simde_func(maddr); \
                                                       \
        V128 result;                                   \
        SIMDE_V128_TO_SIMD_V128(simde_result, result); \
                                                       \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result); \
    } while (0)

                    case SIMD_v128_load8_splat:
                    {
                        SIMD_LOAD_SPLAT_OP(simde_wasm_v128_load8_splat, 8);
                        break;
                    }
                    case SIMD_v128_load16_splat:
                    {
                        SIMD_LOAD_SPLAT_OP(simde_wasm_v128_load16_splat, 16);
                        break;
                    }
                    case SIMD_v128_load32_splat:
                    {
                        SIMD_LOAD_SPLAT_OP(simde_wasm_v128_load32_splat, 32);
                        break;
                    }
                    case SIMD_v128_load64_splat:
                    {
                        SIMD_LOAD_SPLAT_OP(simde_wasm_v128_load64_splat, 64);
                        break;
                    }
                    case SIMD_v128_store:
                    {
                        uint32 offset, addr;
                        offset = read_uint32(frame_ip);
                        V128 data = POP_V128();
                        addr = POP_I32();

                        CHECK_MEMORY_OVERFLOW(16);
                        STORE_V128(maddr, data);
                        break;
                    }

                    /* Basic */
                    case SIMD_v128_const:
                    {
                        uint8 *orig_ip = frame_ip;

                        frame_ip += sizeof(V128);
                        *p_addr_ret = GET_OFFSET();

                        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, *(V128 *)orig_ip);
                        break;
                    }
                    /* TODO: Add a faster SIMD implementation */
                    case SIMD_v8x16_shuffle:
                    {
                        V128 indices;
                        bh_memcpy_s(&indices, sizeof(V128), frame_ip,
                                    sizeof(V128));
                        frame_ip += sizeof(V128);

                        V128 v2 = POP_V128();
                        V128 v1 = POP_V128();
                        *p_addr_ret = GET_OFFSET();

                        V128 result;
                        for (int i = 0; i < 16; i++) {
                            uint8_t index = indices.i8x16[i];
                            if (index < 16) {
                                result.i8x16[i] = v1.i8x16[index];
                            }
                            else {
                                result.i8x16[i] = v2.i8x16[index - 16];
                            }
                        }

                        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);
                        break;
                    }
                    case SIMD_v8x16_swizzle:
                    {
                        V128 v2 = POP_V128();
                        V128 v1 = POP_V128();
                        *p_addr_ret = GET_OFFSET();
                        simde_v128_t simde_result = simde_wasm_i8x16_swizzle(
                            SIMD_V128_TO_SIMDE_V128(v1),
                            SIMD_V128_TO_SIMDE_V128(v2));

                        V128 result;
                        SIMDE_V128_TO_SIMD_V128(simde_result, result);

                        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);
                        break;
                    }

                    /* Splat */
#define SIMD_SPLAT_OP(simde_func, pop_func, val_type)  \
    do {                                               \
        val_type val = pop_func();                     \
        *p_addr_ret = GET_OFFSET();                       \
                                                       \
        simde_v128_t simde_result = simde_func(val);   \
                                                       \
        V128 result;                                   \
        SIMDE_V128_TO_SIMD_V128(simde_result, result); \
                                                       \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result); \
    } while (0)

#define SIMD_SPLAT_OP_I32(simde_func) SIMD_SPLAT_OP(simde_func, POP_I32, uint32)
#define SIMD_SPLAT_OP_I64(simde_func) SIMD_SPLAT_OP(simde_func, POP_I64, uint64)
#define SIMD_SPLAT_OP_F32(simde_func) \
    SIMD_SPLAT_OP(simde_func, POP_F32, float32)
#define SIMD_SPLAT_OP_F64(simde_func) \
    SIMD_SPLAT_OP(simde_func, POP_F64, float64)

                    case SIMD_i8x16_splat:
                    {
                        uint32 val = POP_I32();
                        *p_addr_ret = GET_OFFSET();

                        simde_v128_t simde_result = simde_wasm_i8x16_splat(val);

                        V128 result;
                        SIMDE_V128_TO_SIMD_V128(simde_result, result);

                        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);
                        break;
                    }
                    case SIMD_i16x8_splat:
                    {
                        SIMD_SPLAT_OP_I32(simde_wasm_i16x8_splat);
                        break;
                    }
                    case SIMD_i32x4_splat:
                    {
                        SIMD_SPLAT_OP_I32(simde_wasm_i32x4_splat);
                        break;
                    }
                    case SIMD_i64x2_splat:
                    {
                        SIMD_SPLAT_OP_I64(simde_wasm_i64x2_splat);
                        break;
                    }
                    case SIMD_f32x4_splat:
                    {
                        SIMD_SPLAT_OP_F32(simde_wasm_f32x4_splat);
                        break;
                    }
                    case SIMD_f64x2_splat:
                    {
                        SIMD_SPLAT_OP_F64(simde_wasm_f64x2_splat);
                        break;
                    }
#if WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS != 0
#define SIMD_LANE_HANDLE_UNALIGNED_ACCESS()
#else
#define SIMD_LANE_HANDLE_UNALIGNED_ACCESS() *frame_ip++;
#endif
#define SIMD_EXTRACT_LANE_OP(register, return_type, push_elem) \
    do {                                                       \
        uint8 lane = *frame_ip++;                              \
        SIMD_LANE_HANDLE_UNALIGNED_ACCESS();                   \
        V128 v = POP_V128();                                   \
        push_elem((return_type)(v.register[lane]));            \
    } while (0)
#define SIMD_REPLACE_LANE_OP(register, return_type, pop_elem) \
    do {                                                      \
        uint8 lane = *frame_ip++;                             \
        SIMD_LANE_HANDLE_UNALIGNED_ACCESS();                  \
        return_type replacement = pop_elem();                 \
        V128 v = POP_V128();                                  \
        v.register[lane] = replacement;                       \
        *p_addr_ret = GET_OFFSET();                              \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, v);             \
    } while (0)
                    case SIMD_i8x16_extract_lane_s:
                    {
                        SIMD_EXTRACT_LANE_OP(i8x16, int8, PUSH_I32);
                        break;
                    }
                    case SIMD_i8x16_extract_lane_u:
                    {
                        SIMD_EXTRACT_LANE_OP(i8x16, uint8, PUSH_I32);
                        break;
                    }
                    case SIMD_i8x16_replace_lane:
                    {
                        SIMD_REPLACE_LANE_OP(i8x16, int8, POP_I32);
                        break;
                    }
                    case SIMD_i16x8_extract_lane_s:
                    {
                        SIMD_EXTRACT_LANE_OP(i16x8, int16, PUSH_I32);
                        break;
                    }
                    case SIMD_i16x8_extract_lane_u:
                    {
                        SIMD_EXTRACT_LANE_OP(i16x8, uint16, PUSH_I32);
                        break;
                    }
                    case SIMD_i16x8_replace_lane:
                    {
                        SIMD_REPLACE_LANE_OP(i16x8, int16, POP_I32);
                        break;
                    }
                    case SIMD_i32x4_extract_lane:
                    {
                        SIMD_EXTRACT_LANE_OP(i32x4, int32, PUSH_I32);
                        break;
                    }
                    case SIMD_i32x4_replace_lane:
                    {
                        SIMD_REPLACE_LANE_OP(i32x4, int32, POP_I32);
                        break;
                    }
                    case SIMD_i64x2_extract_lane:
                    {
                        SIMD_EXTRACT_LANE_OP(i64x2, int64, PUSH_I64);
                        break;
                    }
                    case SIMD_i64x2_replace_lane:
                    {
                        SIMD_REPLACE_LANE_OP(i64x2, int64, POP_I64);
                        break;
                    }
                    case SIMD_f32x4_extract_lane:
                    {
                        SIMD_EXTRACT_LANE_OP(f32x4, float32, PUSH_F32);
                        break;
                    }
                    case SIMD_f32x4_replace_lane:
                    {
                        SIMD_REPLACE_LANE_OP(f32x4, float32, POP_F32);
                        break;
                    }
                    case SIMD_f64x2_extract_lane:
                    {
                        SIMD_EXTRACT_LANE_OP(f64x2, float64, PUSH_F64);
                        break;
                    }
                    case SIMD_f64x2_replace_lane:
                    {
                        SIMD_REPLACE_LANE_OP(f64x2, float64, POP_F64);
                        break;
                    }

#define SIMD_DOUBLE_OP(simde_func)                                           \
    do {                                                                     \
        V128 v2 = POP_V128();                                                \
        V128 v1 = POP_V128();                                                \
        *p_addr_ret = GET_OFFSET();                                             \
                                                                             \
        simde_v128_t simde_result = simde_func(SIMD_V128_TO_SIMDE_V128(v1),  \
                                               SIMD_V128_TO_SIMDE_V128(v2)); \
                                                                             \
        V128 result;                                                         \
        SIMDE_V128_TO_SIMD_V128(simde_result, result);                       \
                                                                             \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);                       \
    } while (0)

                    /* i8x16 comparison operations */
                    case SIMD_i8x16_eq:
                    {
                        V128 v2 = POP_V128();
                        V128 v1 = POP_V128();
                        *p_addr_ret = GET_OFFSET();

                        simde_v128_t simde_result =
                            simde_wasm_i8x16_eq(SIMD_V128_TO_SIMDE_V128(v1),
                                                SIMD_V128_TO_SIMDE_V128(v2));

                        V128 result;
                        SIMDE_V128_TO_SIMD_V128(simde_result, result);

                        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);
                        break;
                    }
                    case SIMD_i8x16_ne:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_ne);
                        break;
                    }
                    case SIMD_i8x16_lt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_lt);
                        break;
                    }
                    case SIMD_i8x16_lt_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_lt);
                        break;
                    }
                    case SIMD_i8x16_gt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_gt);
                        break;
                    }
                    case SIMD_i8x16_gt_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_gt);
                        break;
                    }
                    case SIMD_i8x16_le_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_le);
                        break;
                    }
                    case SIMD_i8x16_le_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_le);
                        break;
                    }
                    case SIMD_i8x16_ge_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_ge);
                        break;
                    }
                    case SIMD_i8x16_ge_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_ge);
                        break;
                    }

                    /* i16x8 comparison operations */
                    case SIMD_i16x8_eq:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_eq);
                        break;
                    }
                    case SIMD_i16x8_ne:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_ne);
                        break;
                    }
                    case SIMD_i16x8_lt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_lt);
                        break;
                    }
                    case SIMD_i16x8_lt_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_lt);
                        break;
                    }
                    case SIMD_i16x8_gt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_gt);
                        break;
                    }
                    case SIMD_i16x8_gt_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_gt);
                        break;
                    }
                    case SIMD_i16x8_le_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_le);
                        break;
                    }
                    case SIMD_i16x8_le_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_le);
                        break;
                    }
                    case SIMD_i16x8_ge_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_ge);
                        break;
                    }
                    case SIMD_i16x8_ge_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_ge);
                        break;
                    }

                    /*  i32x4 comparison operations */
                    case SIMD_i32x4_eq:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_eq);
                        break;
                    }
                    case SIMD_i32x4_ne:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_ne);
                        break;
                    }
                    case SIMD_i32x4_lt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_lt);
                        break;
                    }
                    case SIMD_i32x4_lt_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_lt);
                        break;
                    }
                    case SIMD_i32x4_gt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_gt);
                        break;
                    }
                    case SIMD_i32x4_gt_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_gt);
                        break;
                    }
                    case SIMD_i32x4_le_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_le);
                        break;
                    }
                    case SIMD_i32x4_le_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_le);
                        break;
                    }
                    case SIMD_i32x4_ge_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_ge);
                        break;
                    }
                    case SIMD_i32x4_ge_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_ge);
                        break;
                    }

                    /* f32x4 comparison operations */
                    case SIMD_f32x4_eq:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_eq);
                        break;
                    }
                    case SIMD_f32x4_ne:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_ne);
                        break;
                    }
                    case SIMD_f32x4_lt:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_lt);
                        break;
                    }
                    case SIMD_f32x4_gt:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_gt);
                        break;
                    }
                    case SIMD_f32x4_le:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_le);
                        break;
                    }
                    case SIMD_f32x4_ge:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_ge);
                        break;
                    }

                    /* f64x2 comparison operations */
                    case SIMD_f64x2_eq:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_eq);
                        break;
                    }
                    case SIMD_f64x2_ne:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_ne);
                        break;
                    }
                    case SIMD_f64x2_lt:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_lt);
                        break;
                    }
                    case SIMD_f64x2_gt:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_gt);
                        break;
                    }
                    case SIMD_f64x2_le:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_le);
                        break;
                    }
                    case SIMD_f64x2_ge:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_ge);
                        break;
                    }

                    /* v128 bitwise operations */
#define SIMD_V128_BITWISE_OP_COMMON(result_expr_0, result_expr_1) \
    do {                                                          \
        V128 result;                                              \
        result.i64x2[0] = (result_expr_0);                        \
        result.i64x2[1] = (result_expr_1);                        \
        *p_addr_ret = GET_OFFSET();                                  \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);            \
    } while (0)

                    case SIMD_v128_not:
                    {
                        V128 value = POP_V128();
                        SIMD_V128_BITWISE_OP_COMMON(~value.i64x2[0],
                                                    ~value.i64x2[1]);
                        break;
                    }
                    case SIMD_v128_and:
                    {
                        V128 v2 = POP_V128();
                        V128 v1 = POP_V128();
                        SIMD_V128_BITWISE_OP_COMMON(v1.i64x2[0] & v2.i64x2[0],
                                                    v1.i64x2[1] & v2.i64x2[1]);
                        break;
                    }
                    case SIMD_v128_andnot:
                    {
                        V128 v2 = POP_V128();
                        V128 v1 = POP_V128();
                        SIMD_V128_BITWISE_OP_COMMON(
                            v1.i64x2[0] & (~v2.i64x2[0]),
                            v1.i64x2[1] & (~v2.i64x2[1]));
                        break;
                    }
                    case SIMD_v128_or:
                    {
                        V128 v2 = POP_V128();
                        V128 v1 = POP_V128();
                        SIMD_V128_BITWISE_OP_COMMON(v1.i64x2[0] | v2.i64x2[0],
                                                    v1.i64x2[1] | v2.i64x2[1]);
                        break;
                    }
                    case SIMD_v128_xor:
                    {
                        V128 v2 = POP_V128();
                        V128 v1 = POP_V128();
                        SIMD_V128_BITWISE_OP_COMMON(v1.i64x2[0] ^ v2.i64x2[0],
                                                    v1.i64x2[1] ^ v2.i64x2[1]);
                        break;
                    }
                    case SIMD_v128_bitselect:
                    {
                        V128 v1 = POP_V128();
                        V128 v2 = POP_V128();
                        V128 v3 = POP_V128();
                        *p_addr_ret = GET_OFFSET();

                        simde_v128_t simde_result = simde_wasm_v128_bitselect(
                            SIMD_V128_TO_SIMDE_V128(v3),
                            SIMD_V128_TO_SIMDE_V128(v2),
                            SIMD_V128_TO_SIMDE_V128(v1));

                        V128 result;
                        SIMDE_V128_TO_SIMD_V128(simde_result, result);

                        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);
                        break;
                    }
                    case SIMD_v128_any_true:
                    {
                        V128 value = POP_V128();
                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] =
                            value.i64x2[0] != 0 || value.i64x2[1] != 0;
                        break;
                    }

#define SIMD_LOAD_LANE_COMMON(vec, register, lane, width)  \
    do {                                                   \
        *p_addr_ret = GET_OFFSET();                           \
        CHECK_MEMORY_OVERFLOW(width / 8);                  \
        if (width == 64) {                                 \
            vec.register[lane] = GET_I64_FROM_ADDR(maddr); \
        }                                                  \
        else {                                             \
            vec.register[lane] = *(uint##width *)(maddr);  \
        }                                                  \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, vec);        \
    } while (0)

#define SIMD_LOAD_LANE_OP(register, width)                 \
    do {                                                   \
        uint32 offset, addr;                               \
        offset = read_uint32(frame_ip);                    \
        V128 vec = POP_V128();                             \
        addr = POP_I32();                                  \
        int lane = *frame_ip++;                            \
        SIMD_LANE_HANDLE_UNALIGNED_ACCESS();               \
        SIMD_LOAD_LANE_COMMON(vec, register, lane, width); \
    } while (0)

                    case SIMD_v128_load8_lane:
                    {
                        SIMD_LOAD_LANE_OP(i8x16, 8);
                        break;
                    }
                    case SIMD_v128_load16_lane:
                    {
                        SIMD_LOAD_LANE_OP(i16x8, 16);
                        break;
                    }
                    case SIMD_v128_load32_lane:
                    {
                        SIMD_LOAD_LANE_OP(i32x4, 32);
                        break;
                    }
                    case SIMD_v128_load64_lane:
                    {
                        SIMD_LOAD_LANE_OP(i64x2, 64);
                        break;
                    }
#define SIMD_STORE_LANE_OP(register, width)               \
    do {                                                  \
        uint32 offset, addr;                              \
        offset = read_uint32(frame_ip);                   \
        V128 vec = POP_V128();                            \
        addr = POP_I32();                                 \
        int lane = *frame_ip++;                           \
        SIMD_LANE_HANDLE_UNALIGNED_ACCESS();              \
        CHECK_MEMORY_OVERFLOW(width / 8);                 \
        if (width == 64) {                                \
            STORE_I64(maddr, vec.register[lane]);         \
        }                                                 \
        else {                                            \
            *(uint##width *)(maddr) = vec.register[lane]; \
        }                                                 \
    } while (0)

                    case SIMD_v128_store8_lane:
                    {
                        SIMD_STORE_LANE_OP(i8x16, 8);
                        break;
                    }

                    case SIMD_v128_store16_lane:
                    {
                        SIMD_STORE_LANE_OP(i16x8, 16);
                        break;
                    }

                    case SIMD_v128_store32_lane:
                    {
                        SIMD_STORE_LANE_OP(i32x4, 32);
                        break;
                    }

                    case SIMD_v128_store64_lane:
                    {
                        SIMD_STORE_LANE_OP(i64x2, 64);
                        break;
                    }
#define SIMD_LOAD_ZERO_OP(register, width)                 \
    do {                                                   \
        uint32 offset, addr;                               \
        offset = read_uint32(frame_ip);                    \
        addr = POP_I32();                                  \
        int32 lane = 0;                                    \
        V128 vec = { 0 };                                  \
        SIMD_LOAD_LANE_COMMON(vec, register, lane, width); \
    } while (0)

                    case SIMD_v128_load32_zero:
                    {
                        SIMD_LOAD_ZERO_OP(i32x4, 32);
                        break;
                    }
                    case SIMD_v128_load64_zero:
                    {
                        SIMD_LOAD_ZERO_OP(i64x2, 64);
                        break;
                    }

#define SIMD_SINGLE_OP(simde_func)                                           \
    do {                                                                     \
        V128 v1 = POP_V128();                                                \
        *p_addr_ret = GET_OFFSET();                                             \
                                                                             \
        simde_v128_t simde_result = simde_func(SIMD_V128_TO_SIMDE_V128(v1)); \
                                                                             \
        V128 result;                                                         \
        SIMDE_V128_TO_SIMD_V128(simde_result, result);                       \
                                                                             \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);                       \
    } while (0)

                    /* Float conversion */
                    case SIMD_f32x4_demote_f64x2_zero:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_demote_f64x2_zero);
                        break;
                    }
                    case SIMD_f64x2_promote_low_f32x4_zero:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_promote_low_f32x4);
                        break;
                    }

                    /* i8x16 operations */
                    case SIMD_i8x16_abs:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i8x16_abs);
                        break;
                    }
                    case SIMD_i8x16_neg:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i8x16_neg);
                        break;
                    }
                    case SIMD_i8x16_popcnt:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i8x16_popcnt);
                        break;
                    }
                    case SIMD_i8x16_all_true:
                    {
                        V128 v1 = POP_V128();

                        bool result = simde_wasm_i8x16_all_true(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }

                    case SIMD_i8x16_bitmask:
                    {
                        V128 v1 = POP_V128();

                        uint32_t result = simde_wasm_i8x16_bitmask(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }
                    case SIMD_i8x16_narrow_i16x8_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_narrow_i16x8);
                        break;
                    }
                    case SIMD_i8x16_narrow_i16x8_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_narrow_i16x8);
                        break;
                    }
                    case SIMD_f32x4_ceil:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_ceil);
                        break;
                    }
                    case SIMD_f32x4_floor:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_floor);
                        break;
                    }
                    case SIMD_f32x4_trunc:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_trunc);
                        break;
                    }
                    case SIMD_f32x4_nearest:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_nearest);
                        break;
                    }
#define SIMD_LANE_SHIFT(simde_func)                         \
    do {                                                    \
        int32 count = POP_I32();                            \
        V128 v1 = POP_V128();                               \
        *p_addr_ret = GET_OFFSET();                            \
                                                            \
        simde_v128_t simde_result =                         \
            simde_func(SIMD_V128_TO_SIMDE_V128(v1), count); \
                                                            \
        V128 result;                                        \
        SIMDE_V128_TO_SIMD_V128(simde_result, result);      \
                                                            \
        PUT_V128_TO_ADDR(frame_lp + *p_addr_ret, result);      \
    } while (0)
                    case SIMD_i8x16_shl:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i8x16_shl);
                        break;
                    }
                    case SIMD_i8x16_shr_s:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i8x16_shr);
                        break;
                    }
                    case SIMD_i8x16_shr_u:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_u8x16_shr);
                        break;
                    }
                    case SIMD_i8x16_add:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_add);
                        break;
                    }
                    case SIMD_i8x16_add_sat_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_add_sat);
                        break;
                    }
                    case SIMD_i8x16_add_sat_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_add_sat);
                        break;
                    }
                    case SIMD_i8x16_sub:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_sub);
                        break;
                    }
                    case SIMD_i8x16_sub_sat_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_sub_sat);
                        break;
                    }
                    case SIMD_i8x16_sub_sat_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_sub_sat);
                        break;
                    }
                    case SIMD_f64x2_ceil:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_ceil);
                        break;
                    }
                    case SIMD_f64x2_floor:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_floor);
                        break;
                    }
                    case SIMD_i8x16_min_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_min);
                        break;
                    }
                    case SIMD_i8x16_min_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_min);
                        break;
                    }
                    case SIMD_i8x16_max_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i8x16_max);
                        break;
                    }
                    case SIMD_i8x16_max_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_max);
                        break;
                    }
                    case SIMD_f64x2_trunc:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_trunc);
                        break;
                    }
                    case SIMD_i8x16_avgr_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u8x16_avgr);
                        break;
                    }
                    case SIMD_i16x8_extadd_pairwise_i8x16_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i16x8_extadd_pairwise_i8x16);
                        break;
                    }
                    case SIMD_i16x8_extadd_pairwise_i8x16_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u16x8_extadd_pairwise_u8x16);
                        break;
                    }
                    case SIMD_i32x4_extadd_pairwise_i16x8_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i32x4_extadd_pairwise_i16x8);
                        break;
                    }
                    case SIMD_i32x4_extadd_pairwise_i16x8_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u32x4_extadd_pairwise_u16x8);
                        break;
                    }

                    /* i16x8 operations */
                    case SIMD_i16x8_abs:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i16x8_abs);
                        break;
                    }
                    case SIMD_i16x8_neg:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i16x8_neg);
                        break;
                    }
                    case SIMD_i16x8_q15mulr_sat_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_q15mulr_sat);
                        break;
                    }
                    case SIMD_i16x8_all_true:
                    {
                        V128 v1 = POP_V128();

                        bool result = simde_wasm_i16x8_all_true(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }
                    case SIMD_i16x8_bitmask:
                    {
                        V128 v1 = POP_V128();

                        uint32_t result = simde_wasm_i16x8_bitmask(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }
                    case SIMD_i16x8_narrow_i32x4_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_narrow_i32x4);
                        break;
                    }
                    case SIMD_i16x8_narrow_i32x4_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_narrow_i32x4);
                        break;
                    }
                    case SIMD_i16x8_extend_low_i8x16_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i16x8_extend_low_i8x16);
                        break;
                    }
                    case SIMD_i16x8_extend_high_i8x16_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i16x8_extend_high_i8x16);
                        break;
                    }
                    case SIMD_i16x8_extend_low_i8x16_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u16x8_extend_low_u8x16);
                        break;
                    }
                    case SIMD_i16x8_extend_high_i8x16_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u16x8_extend_high_u8x16);
                        break;
                    }
                    case SIMD_i16x8_shl:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i16x8_shl);
                        break;
                    }
                    case SIMD_i16x8_shr_s:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i16x8_shr);
                        break;
                    }
                    case SIMD_i16x8_shr_u:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_u16x8_shr);
                        break;
                    }
                    case SIMD_i16x8_add:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_add);
                        break;
                    }
                    case SIMD_i16x8_add_sat_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_add_sat);
                        break;
                    }
                    case SIMD_i16x8_add_sat_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_add_sat);
                        break;
                    }
                    case SIMD_i16x8_sub:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_sub);
                        break;
                    }
                    case SIMD_i16x8_sub_sat_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_sub_sat);
                        break;
                    }
                    case SIMD_i16x8_sub_sat_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_sub_sat);
                        break;
                    }
                    case SIMD_f64x2_nearest:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_nearest);
                        break;
                    }
                    case SIMD_i16x8_mul:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_mul);
                        break;
                    }
                    case SIMD_i16x8_min_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_min);
                        break;
                    }
                    case SIMD_i16x8_min_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_min);
                        break;
                    }
                    case SIMD_i16x8_max_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_max);
                        break;
                    }
                    case SIMD_i16x8_max_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_max);
                        break;
                    }
                    case SIMD_i16x8_avgr_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_avgr);
                        break;
                    }
                    case SIMD_i16x8_extmul_low_i8x16_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_extmul_low_i8x16);
                        break;
                    }
                    case SIMD_i16x8_extmul_high_i8x16_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i16x8_extmul_high_i8x16);
                        break;
                    }
                    case SIMD_i16x8_extmul_low_i8x16_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_extmul_low_u8x16);
                        break;
                    }
                    case SIMD_i16x8_extmul_high_i8x16_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u16x8_extmul_high_u8x16);
                        break;
                    }

                    /* i32x4 operations */
                    case SIMD_i32x4_abs:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i32x4_abs);
                        break;
                    }
                    case SIMD_i32x4_neg:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i32x4_neg);
                        break;
                    }
                    case SIMD_i32x4_all_true:
                    {
                        V128 v1 = POP_V128();

                        bool result = simde_wasm_i32x4_all_true(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }
                    case SIMD_i32x4_bitmask:
                    {
                        V128 v1 = POP_V128();

                        uint32_t result = simde_wasm_i32x4_bitmask(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }
                    case SIMD_i32x4_extend_low_i16x8_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i32x4_extend_low_i16x8);
                        break;
                    }
                    case SIMD_i32x4_extend_high_i16x8_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i32x4_extend_high_i16x8);
                        break;
                    }
                    case SIMD_i32x4_extend_low_i16x8_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u32x4_extend_low_u16x8);
                        break;
                    }
                    case SIMD_i32x4_extend_high_i16x8_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u32x4_extend_high_u16x8);
                        break;
                    }
                    case SIMD_i32x4_shl:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i32x4_shl);
                        break;
                    }
                    case SIMD_i32x4_shr_s:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i32x4_shr);
                        break;
                    }
                    case SIMD_i32x4_shr_u:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_u32x4_shr);
                        break;
                    }
                    case SIMD_i32x4_add:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_add);
                        break;
                    }
                    case SIMD_i32x4_sub:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_sub);
                        break;
                    }
                    case SIMD_i32x4_mul:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_mul);
                        break;
                    }
                    case SIMD_i32x4_min_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_min);
                        break;
                    }
                    case SIMD_i32x4_min_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_min);
                        break;
                    }
                    case SIMD_i32x4_max_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_max);
                        break;
                    }
                    case SIMD_i32x4_max_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_max);
                        break;
                    }
                    case SIMD_i32x4_dot_i16x8_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_dot_i16x8);
                        break;
                    }
                    case SIMD_i32x4_extmul_low_i16x8_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_extmul_low_i16x8);
                        break;
                    }
                    case SIMD_i32x4_extmul_high_i16x8_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i32x4_extmul_high_i16x8);
                        break;
                    }
                    case SIMD_i32x4_extmul_low_i16x8_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_extmul_low_u16x8);
                        break;
                    }
                    case SIMD_i32x4_extmul_high_i16x8_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u32x4_extmul_high_u16x8);
                        break;
                    }

                    /* i64x2 operations */
                    case SIMD_i64x2_abs:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i64x2_abs);
                        break;
                    }
                    case SIMD_i64x2_neg:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i64x2_neg);
                        break;
                    }
                    case SIMD_i64x2_all_true:
                    {
                        V128 v1 = POP_V128();

                        bool result = simde_wasm_i64x2_all_true(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }
                    case SIMD_i64x2_bitmask:
                    {
                        V128 v1 = POP_V128();

                        uint32_t result = simde_wasm_i64x2_bitmask(
                            SIMD_V128_TO_SIMDE_V128(v1));

                        *p_addr_ret = GET_OFFSET();
                        frame_lp[*p_addr_ret] = result;
                        break;
                    }
                    case SIMD_i64x2_extend_low_i32x4_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i64x2_extend_low_i32x4);
                        break;
                    }
                    case SIMD_i64x2_extend_high_i32x4_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i64x2_extend_high_i32x4);
                        break;
                    }
                    case SIMD_i64x2_extend_low_i32x4_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u64x2_extend_low_u32x4);
                        break;
                    }
                    case SIMD_i64x2_extend_high_i32x4_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u64x2_extend_high_u32x4);
                        break;
                    }
                    case SIMD_i64x2_shl:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i64x2_shl);
                        break;
                    }
                    case SIMD_i64x2_shr_s:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_i64x2_shr);
                        break;
                    }
                    case SIMD_i64x2_shr_u:
                    {
                        SIMD_LANE_SHIFT(simde_wasm_u64x2_shr);
                        break;
                    }
                    case SIMD_i64x2_add:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_add);
                        break;
                    }
                    case SIMD_i64x2_sub:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_sub);
                        break;
                    }
                    case SIMD_i64x2_mul:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_mul);
                        break;
                    }
                    case SIMD_i64x2_eq:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_eq);
                        break;
                    }
                    case SIMD_i64x2_ne:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_ne);
                        break;
                    }
                    case SIMD_i64x2_lt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_lt);
                        break;
                    }
                    case SIMD_i64x2_gt_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_gt);
                        break;
                    }
                    case SIMD_i64x2_le_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_le);
                        break;
                    }
                    case SIMD_i64x2_ge_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_ge);
                        break;
                    }
                    case SIMD_i64x2_extmul_low_i32x4_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_extmul_low_i32x4);
                        break;
                    }
                    case SIMD_i64x2_extmul_high_i32x4_s:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_i64x2_extmul_high_i32x4);
                        break;
                    }
                    case SIMD_i64x2_extmul_low_i32x4_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u64x2_extmul_low_u32x4);
                        break;
                    }
                    case SIMD_i64x2_extmul_high_i32x4_u:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_u64x2_extmul_high_u32x4);
                        break;
                    }

                    /* f32x4 opertions */
                    case SIMD_f32x4_abs:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_abs);
                        break;
                    }
                    case SIMD_f32x4_neg:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_neg);
                        break;
                    }
                    case SIMD_f32x4_sqrt:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_sqrt);
                        break;
                    }
                    case SIMD_f32x4_add:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_add);
                        break;
                    }
                    case SIMD_f32x4_sub:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_sub);
                        break;
                    }
                    case SIMD_f32x4_mul:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_mul);
                        break;
                    }
                    case SIMD_f32x4_div:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_div);
                        break;
                    }
                    case SIMD_f32x4_min:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_min);
                        break;
                    }
                    case SIMD_f32x4_max:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_max);
                        break;
                    }
                    case SIMD_f32x4_pmin:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_pmin);
                        break;
                    }
                    case SIMD_f32x4_pmax:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f32x4_pmax);
                        break;
                    }

                    /* f64x2 operations */
                    case SIMD_f64x2_abs:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_abs);
                        break;
                    }
                    case SIMD_f64x2_neg:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_neg);
                        break;
                    }
                    case SIMD_f64x2_sqrt:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_sqrt);
                        break;
                    }
                    case SIMD_f64x2_add:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_add);
                        break;
                    }
                    case SIMD_f64x2_sub:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_sub);
                        break;
                    }
                    case SIMD_f64x2_mul:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_mul);
                        break;
                    }
                    case SIMD_f64x2_div:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_div);
                        break;
                    }
                    case SIMD_f64x2_min:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_min);
                        break;
                    }
                    case SIMD_f64x2_max:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_max);
                        break;
                    }
                    case SIMD_f64x2_pmin:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_pmin);
                        break;
                    }
                    case SIMD_f64x2_pmax:
                    {
                        SIMD_DOUBLE_OP(simde_wasm_f64x2_pmax);
                        break;
                    }

                    /* Conversion operations */
                    case SIMD_i32x4_trunc_sat_f32x4_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i32x4_trunc_sat_f32x4);
                        break;
                    }
                    case SIMD_i32x4_trunc_sat_f32x4_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u32x4_trunc_sat_f32x4);
                        break;
                    }
                    case SIMD_f32x4_convert_i32x4_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_convert_i32x4);
                        break;
                    }
                    case SIMD_f32x4_convert_i32x4_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f32x4_convert_u32x4);
                        break;
                    }
                    case SIMD_i32x4_trunc_sat_f64x2_s_zero:
                    {
                        SIMD_SINGLE_OP(simde_wasm_i32x4_trunc_sat_f64x2_zero);
                        break;
                    }
                    case SIMD_i32x4_trunc_sat_f64x2_u_zero:
                    {
                        SIMD_SINGLE_OP(simde_wasm_u32x4_trunc_sat_f64x2_zero);
                        break;
                    }
                    case SIMD_f64x2_convert_low_i32x4_s:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_convert_low_i32x4);
                        break;
                    }
                    case SIMD_f64x2_convert_low_i32x4_u:
                    {
                        SIMD_SINGLE_OP(simde_wasm_f64x2_convert_low_u32x4);
                        break;
                    }

                    default:
                        wasm_set_exception(module_inst, "unsupported SIMD opcode");
                }
                return frame_ip;

#else
    wasm_set_exception(ctx->module_inst, "unsupported SIMD opcode");
    return false;
#endif

#if !defined(OS_ENABLE_HW_BOUND_CHECK)              \
    || WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS == 0 \
    || WASM_ENABLE_BULK_MEMORY != 0
    out_of_bounds:
        wasm_set_exception(module_inst, "out of bounds memory access");
    return frame_ip;
#endif

    }
#endif /* WASM_ENABLE_SIMD */