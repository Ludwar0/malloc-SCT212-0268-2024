# Custom Heap Allocator

**Student:** Emmanuel Ludwaro  
**Registration number:** SCT212-0268/2024  
**Unit:** ICS 2305 – Systems Programming

## Build and run

```sh
make          # builds the supplied test harness
make test     # runs all 8 groups / 29 checks
make bench    # builds and runs the fixed 50,000-operation benchmark
make clean
```

The supplied `myalloc.h` and `test_myalloc.c` are included unchanged.

## Task checklist

| Task | Status | Work completed |
|---|---|---|
| 1. Header and helpers | Complete | 16-byte aligned header/payload helpers and magic value |
| 2. Heap growth | Complete | `sbrk()`, alignment padding, 64 KiB minimum chunks, head/tail |
| 3. First-fit allocation | Complete | `find_fit()` and `my_malloc()` |
| 4. Splitting | Complete | Split when a header and 16-byte payload will fit |
| 5. Free and coalescing | Complete | Invalid/double-free diagnostics and physical-adjacency merge |
| 6. Calloc and realloc | Complete | Overflow guard, zeroing, resize and content preservation |
| 7. Best-fit and diagnostics | Complete | Policy selection, stats, dump and consistency checker |
| 8. Experiment and report | Complete | Fixed-seed benchmark and measured comparison in `REPORT.md` |

## Design explanation

Every allocator block begins with a metadata header containing the payload size, a pointer to the next block, a free/used flag and a magic value. The header size is rounded up to a multiple of 16 bytes; requested payload sizes are rounded up in the same way. Consequently, if the start of a newly obtained region is aligned, the payload that follows its header is aligned as well. The allocator stores blocks in increasing address order using `head` and `tail` pointers.

`my_malloc()` searches the list using the selected placement policy. First-fit stops at the first sufficiently large free block, while best-fit scans the full list and selects the smallest suitable block. A large free block is split only when the remainder can hold another header and at least 16 bytes of payload. When memory is needed, `grow()` obtains at least 64 KiB from `sbrk()`, with padding to align the header. `my_free()` first checks that the supplied address is an exact payload pointer in the list, then checks its magic value and allocation state. Coalescing joins free list neighbors only when the end address of the first payload exactly meets the next header; this avoids merging across gaps introduced by other users of the process break. `heap_check()` checks alignment, ordering, overlap, metadata and unmerged adjacent free blocks.

## Test results

The provided harness was run locally with `make test`: **29 passed, 0 failed**. The benchmark was run with the fixed seed shown in `bench.c`; measured figures are in `REPORT.md`. Re-run both commands on the target environment before submission because timings and memory layout can vary.

## Written questions

1. **Why must both header size and payload sizes be multiples of 16?** The returned payload address is the block start plus the header size. If both are multiples of 16, an aligned block start produces an aligned payload. Rounding each allocation size also keeps the following block header on the required boundary.

2. **Why can `printf()` inside `my_malloc()` cause recursion?** Formatted I/O may allocate internal buffers using the C library allocator. If that path calls this allocator again, it can re-enter `my_malloc()` while the original allocation is still in progress. The allocator therefore formats diagnostic text locally and writes it with `write()`.

3. **What field enables O(1) access to the previous neighbor, and what is the technique?** A `prev` pointer in each block header gives direct access to the previous list node. This is a doubly linked list.

4. **What is internal versus external fragmentation?** Internal fragmentation is unused space inside an allocated block, for example the bytes added when a request is rounded up to 16-byte alignment or when a tiny remainder cannot be split. External fragmentation is free memory spread across separate holes; it is reduced here by coalescing physically adjacent free blocks.

5. **Why is sbrk memory not returned, and how could it be returned?** The allocator grows the process break but does not shrink it, because only a free block at the very end of the process heap can safely be returned without leaving a hole. A tail-trimming extension could move the break backwards with `brk()` or a negative `sbrk()` after verifying that the last block is free and physically reaches the current break.

6. **Why can consecutive list blocks not be adjacent in memory, and what happens?** The C library or another component can make its own allocation between two calls to this allocator, creating a gap in the process address space. Coalescing checks actual addresses and leaves those blocks separate when the first payload does not end at the next header.

## Known limitations

The allocator is single-threaded and does not use locks. It does not return whole free chunks to the operating system, and it uses a linear scan for placement and pointer validation. The allocator intentionally avoids defining the standard `malloc` family. The benchmark reports allocator heap statistics after all its slots have been freed; therefore a fully coalesced workload can produce a single large free block and a zero external-fragmentation value. `sbrk()` is a legacy interface and can conflict with other allocators in a process, which is why this project is intended as a controlled teaching exercise
