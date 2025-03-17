#if WASM_ENABLE_THREAD_MGR == 0
#define get_linear_mem_size() linear_mem_size
#else
/**
 * Load memory data size in each time boundary check in
 * multi-threading mode since it may be changed by other
 * threads in memory.grow
 */
#define get_linear_mem_size() GET_LINEAR_MEMORY_SIZE(memory)
#endif

/* #define HANDLE_OP(opcode) HANDLE_##opcode:printf(#opcode"\n"); */
#if WASM_ENABLE_OPCODE_COUNTER != 0
#define HANDLE_OP(opcode) HANDLE_##opcode : opcode_table[opcode].count++;
#else
#define HANDLE_OP(opcode) HANDLE_##opcode:
#endif

#if WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS != 0
#define GET_OPCODE() opcode = *frame_ip++;
#else
#define GET_OPCODE()    \
    opcode = *frame_ip; \
    frame_ip += 2;
#endif

#define read_uint32(p) \
    (p += sizeof(uint32), LOAD_U32_WITH_2U16S(p - sizeof(uint32)))

#if WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS != 0
#define LOAD_U32_WITH_2U16S(addr) (*(uint32 *)(addr))
#define LOAD_PTR(addr) (*(void **)(addr))
#else /* else of WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS */
static inline uint32
LOAD_U32_WITH_2U16S(void *addr)
{
    union {
        uint32 val;
        uint16 u16[2];
    } u;

    bh_assert(((uintptr_t)addr & 1) == 0);
    u.u16[0] = ((uint16 *)addr)[0];
    u.u16[1] = ((uint16 *)addr)[1];
    return u.val;
}
#if UINTPTR_MAX == UINT32_MAX
#define LOAD_PTR(addr) ((void *)LOAD_U32_WITH_2U16S(addr))
#elif UINTPTR_MAX == UINT64_MAX
static inline void *
LOAD_PTR(void *addr)
{
    uintptr_t addr1 = (uintptr_t)addr;
    union {
        void *val;
        uint32 u32[2];
        uint16 u16[4];
    } u;

    bh_assert(((uintptr_t)addr & 1) == 0);
    if ((addr1 & (uintptr_t)7) == 0)
        return *(void **)addr;

    if ((addr1 & (uintptr_t)3) == 0) {
        u.u32[0] = ((uint32 *)addr)[0];
        u.u32[1] = ((uint32 *)addr)[1];
    }
    else {
        u.u16[0] = ((uint16 *)addr)[0];
        u.u16[1] = ((uint16 *)addr)[1];
        u.u16[2] = ((uint16 *)addr)[2];
        u.u16[3] = ((uint16 *)addr)[3];
    }
    return u.val;
}
#endif /* end of UINTPTR_MAX */
#endif /* end of WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS */

#define POP_I32() (*(int32 *)(frame_lp + GET_OFFSET()))

#define POP_F32() (*(float32 *)(frame_lp + GET_OFFSET()))

#define POP_I64() (GET_I64_FROM_ADDR(frame_lp + GET_OFFSET()))

#define POP_V128() (GET_V128_FROM_ADDR(frame_lp + GET_OFFSET()))

#define POP_F64() (GET_F64_FROM_ADDR(frame_lp + GET_OFFSET()))

#define GET_OFFSET() (frame_ip += 2, *(int16 *)(frame_ip - 2))

#if !defined(OS_ENABLE_HW_BOUND_CHECK) \
    || WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS == 0
#define CHECK_MEMORY_OVERFLOW(bytes)                                           \
    do {                                                                       \
        uint64 offset1 = (uint64)offset + (uint64)addr;                        \
        CHECK_SHARED_HEAP_OVERFLOW(offset1, bytes, maddr)                      \
        if (disable_bounds_checks || offset1 + bytes <= get_linear_mem_size()) \
            /* If offset1 is in valid range, maddr must also                   \
                be in valid range, no need to check it again. */               \
            maddr = memory->memory_data + offset1;                             \
        else                                                                   \
            goto out_of_bounds;                                                \
    } while (0)

#define CHECK_BULK_MEMORY_OVERFLOW(start, bytes, maddr)                        \
    do {                                                                       \
        uint64 offset1 = (uint32)(start);                                      \
        CHECK_SHARED_HEAP_OVERFLOW(offset1, bytes, maddr)                      \
        if (disable_bounds_checks || offset1 + bytes <= get_linear_mem_size()) \
            /* App heap space is not valid space for                           \
               bulk memory operation */                                        \
            maddr = memory->memory_data + offset1;                             \
        else                                                                   \
            goto out_of_bounds;                                                \
    } while (0)
#else
#define CHECK_MEMORY_OVERFLOW(bytes)                      \
    do {                                                  \
        uint64 offset1 = (uint64)offset + (uint64)addr;   \
        CHECK_SHARED_HEAP_OVERFLOW(offset1, bytes, maddr) \
        maddr = memory->memory_data + offset1;            \
    } while (0)

#define CHECK_BULK_MEMORY_OVERFLOW(start, bytes, maddr)   \
    do {                                                  \
        uint64 offset1 = (uint32)(start);                 \
        CHECK_SHARED_HEAP_OVERFLOW(offset1, bytes, maddr) \
        maddr = memory->memory_data + offset1;            \
    } while (0)
#endif /* !defined(OS_ENABLE_HW_BOUND_CHECK) \
          || WASM_CPU_SUPPORTS_UNALIGNED_ADDR_ACCESS == 0 */

#if WASM_ENABLE_SHARED_HEAP != 0
#define app_addr_in_shared_heap(app_addr, bytes)        \
    (shared_heap && (app_addr) >= shared_heap_start_off \
     && (app_addr) <= shared_heap_end_off - bytes + 1)

#define shared_heap_addr_app_to_native(app_addr, native_addr) \
    native_addr = shared_heap_base_addr + ((app_addr) - shared_heap_start_off)

#define CHECK_SHARED_HEAP_OVERFLOW(app_addr, bytes, native_addr) \
    if (app_addr_in_shared_heap(app_addr, bytes))                \
        shared_heap_addr_app_to_native(app_addr, native_addr);   \
    else
#else
#define CHECK_SHARED_HEAP_OVERFLOW(app_addr, bytes, native_addr)
#endif

#define PUSH_I32(value)                              \
    do {                                             \
        *(int32 *)(frame_lp + GET_OFFSET()) = value; \
    } while (0)

#define PUSH_F32(value)                                \
    do {                                               \
        *(float32 *)(frame_lp + GET_OFFSET()) = value; \
    } while (0)

#define PUSH_I64(value)                             \
    do {                                            \
        uint32 *addr_tmp = frame_lp + GET_OFFSET(); \
        PUT_I64_TO_ADDR(addr_tmp, value);           \
    } while (0)

#define PUSH_F64(value)                             \
    do {                                            \
        uint32 *addr_tmp = frame_lp + GET_OFFSET(); \
        PUT_F64_TO_ADDR(addr_tmp, value);           \
    } while (0)

#define PUSH_REF(value)                   \
    do {                                  \
        uint32 *addr_tmp;                 \
        opnd_off = GET_OFFSET();          \
        addr_tmp = frame_lp + opnd_off;   \
        PUT_REF_TO_ADDR(addr_tmp, value); \
        SET_FRAME_REF(opnd_off);          \
    } while (0)

#define PUSH_I31REF(value)                \
    do {                                  \
        uint32 *addr_tmp;                 \
        opnd_off = GET_OFFSET();          \
        addr_tmp = frame_lp + opnd_off;   \
        PUT_REF_TO_ADDR(addr_tmp, value); \
    } while (0)

#define SYNC_ALL_TO_FRAME()   \
    do {                      \
        frame->ip = frame_ip; \
        SYNC_FRAME_REF();     \
    } while (0)

#if WASM_ENABLE_GC != 0
#define SYNC_FRAME_REF() frame->frame_ref = frame_ref
#define UPDATE_FRAME_REF() frame_ref = frame->frame_ref
#else
#define SYNC_FRAME_REF() (void)0
#define UPDATE_FRAME_REF() (void)0
#endif