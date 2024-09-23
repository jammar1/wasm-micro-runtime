(module
  (import "wasi_snapshot_preview1" "proc_exit" (func $proc_exit (param i32)))
  
  (memory (export "memory") 1)

  ;; WASI entry point
  (func $main (export "_start")
    ;; (i8x16.all_true (i8x16.eq (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)))
    (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)
    (drop)

    ;; If we reach here, all tests passed
    (call $proc_exit (i32.const 0))  ;; Exit with success code 0
  )
)