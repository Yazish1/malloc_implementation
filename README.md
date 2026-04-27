# libmalloc

A memory allocator implemented in C from scratch, built to understand the internals of dynamic memory allocation. Implements the full standard allocator API (`malloc`, `free`, `realloc`, `calloc`) and benchmarks against glibc across multiple allocation patterns.

---

## Features

- **First-fit allocation** — scans the free list and returns the first block that fits the requested size
- **Block splitting** — when a free block is larger than needed, carves off the remainder and returns it to the free list, minimising wasted space
- **Free block coalescing** — on every `free`, adjacent free blocks are merged into a single larger block to reduce fragmentation
- **`mmap`-based allocation** — uses `mmap` instead of the deprecated `sbrk` for requesting memory from the OS, with a minimum chunk size of 4096 bytes (one page) to reduce syscall frequency
- **16-byte alignment** — block headers are padded to 32 bytes, guaranteeing all returned pointers are 16-byte aligned.
- **Full API** — `my_malloc`, `my_free`, `my_realloc`, `my_calloc` all implemented and self-contained (no internal calls to glibc)

---

## Implementation Details

### Block Header

Each allocation is preceded by a metadata header stored in the heap itself:

```c
struct block_header {
    struct block_header *next;  // 8 bytes — intrusive linked list
    size_t size;                // 8 bytes — usable bytes in this block
    int isFree;                 // 4 bytes — free flag
    char padding[12];           // 12 bytes — pad to 32 for 16-byte alignment
};
```

The free list is a singly linked list threaded through these headers. No separate metadata storage is needed — the headers live inside the allocated memory regions.

### Allocation Strategy

`my_malloc` follows this path:

1. If the free list is empty, call `request_extra` to `mmap` a new chunk
2. Otherwise, walk the free list with `find_next_free` looking for a block with `size >= requested && isFree`
3. If found and large enough to split (`block->size - size >= sizeof(header) + 1`), call `split_block` to carve off the remainder
4. If no free block fits, call `request_extra` to extend the heap

### Block Splitting

When a free block is significantly larger than needed, `split_block` carves a new header out of the leftover space:

```
BEFORE:
[ header | size=4000 | free=1 ][ ........4000 bytes........ ]

AFTER malloc(64):
[ header | size=64 | free=0 ][ 64 bytes ][ header | size=3904 | free=1 ][ 3904 bytes ]
```

The remainder header is written directly into the unused bytes using pointer arithmetic:

```c
struct block_header *leftover = (struct block_header *)((char *)block + sizeof(struct block_header) + size);
```

### Coalescing

On every `my_free`, the entire free list is walked and adjacent free blocks are merged:

```c
if (current->isFree && current->next->isFree) {
    current->size += current->next->size + sizeof(struct block_header);
    current->next = current->next->next;
}
```

This is O(n) per free call, making bulk-free patterns O(n²) overall — a known limitation, addressable with lazy coalescing.

### Page-Sized Chunks

`request_extra` always requests at least 4096 bytes from the OS, even for small allocations:

```c
size_t chunk = size + sizeof(struct block_header);
if (chunk < 4096) chunk = 4096;
```

The surplus is immediately split off and added to the free list, reducing the number of `mmap` syscalls across many small allocations.

---

## Building

```bash
# benchmark binary
gcc -Wall -O2 -o func func.c
```

---

## Benchmark Results

Benchmarked on WSL2 (x86-64 Linux) against glibc using three allocation patterns:

| Pattern                               | My Allocator | glibc     |
| ------------------------------------- | ------------ | --------- |
| Churn (100k alloc/free, 64B)          | 0.000235s    | 0.000000s |
| Ramp (10k alloc then 10k free, 64B)   | 0.182229s    | 0.001062s |
| Mixed sizes (100k alloc/free, 8–520B) | 0.001306s    | 0.001164s |

**Fragmentation ratio** (heap bytes allocated / bytes in use) measured during ramp test: **1.53**

## Known Limitations and Future Work

- **O(n²) coalescing** — lazy coalescing would fix the ramp pattern performance
- **Not thread-safe** — no mutex around the free list; would need `pthread_mutex_t` or a lock-free structure for concurrent use
- **First-fit fragmentation** — best-fit search would reduce the 1.53 fragmentation ratio at the cost of slower allocation
- **No `munmap` on free** — freed pages are never returned to the OS; a production allocator would `munmap` large unused regions
