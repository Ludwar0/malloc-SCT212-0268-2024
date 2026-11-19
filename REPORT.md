# Allocator experiment report

**Student:** Manu Ludwaro — **SCT212-0268/2024**

## Method

`bench.c` performs 50,000 allocation/free decisions over 512 slots. Request sizes are from 1 to 4096 bytes, and each policy is run with `srand(20261120)`. The same deterministic operation sequence is then used for the C library `malloc()`/`free()` baseline. Elapsed time is measured with `clock_gettime(CLOCK_MONOTONIC)`. All remaining slots are freed before allocator statistics are collected.

## Measurements

One run in the provided Linux environment produced the following results. Times are measurements from that run, not guaranteed performance figures on another machine.

| Allocator | Time (s) | heap_bytes | free_bytes | free_blocks | largest_free | External fragmentation |
|---|---:|---:|---:|---:|---:|---:|
| FIT_FIRST | 0.067603 | 721248 | 721216 | 1 | 721216 | 0.0000 |
| FIT_BEST | 0.088459 | 721248 | 721216 | 1 | 721216 | 0.0000 |
| glibc malloc/free | 0.004541 | Not exposed by this harness | Not exposed | Not exposed | Not exposed | Not calculated |

External fragmentation is calculated as `1 - largest_free / free_bytes`. In this run, all custom allocator blocks were free and adjacent blocks had coalesced into one block at the end of the workload, so the measured external-fragmentation value was zero for both policies.

## Discussion

For this measured workload, first-fit is the better choice. It completed the operations faster than best-fit, while the final free-space statistics were identical after the remaining allocations had been released and adjacent blocks coalesced. Best-fit has to inspect the whole free list to find the smallest suitable hole, whereas first-fit can stop as soon as it finds a usable block. The glibc baseline was substantially faster in this run. That is expected: this allocator is a small educational implementation with linear list searches, simple metadata and no sophisticated caching or per-thread arenas. The measurements should not be treated as a universal ranking; different request patterns and the point at which statistics are sampled can change fragmentation and timing. For this assignment's fixed experiment, I would choose first-fit because its simpler search strategy gave the better measured time without a worse final fragmentation result.
