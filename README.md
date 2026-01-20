# Custom Heap Memory Allocator in C

A thread-safe implementation of dynamic memory allocation functions (`malloc`, `free`, `realloc`, and `calloc`) with advanced optimization techniques.

## Overview

This allocator implements a custom heap manager using the `sbrk` system call for direct heap-to-virtual memory mapping. It features a doubly-linked free-list architecture with first-fit allocation strategy, providing efficient memory management with O(1) metadata access.

## Key Features

### Thread Safety
- Protected by POSIX mutex (`pthread_mutex_t`)
- All malloc/free operations are synchronized
- Safe for concurrent multi-threaded applications

### Metadata Alignment
- 16-byte alignment for cache-friendly access patterns
- Reduces cache misses and improves performance
- Ensures proper memory boundary alignment

### Deferred Coalescing
- Batches merge operations to reduce overhead
- Triggered after 10 free operations (configurable threshold)
- More efficient than immediate coalescing during high-frequency allocations

### Block Splitting Algorithm
- Splits oversized blocks to allocate exact size required
- Mitigates internal fragmentation
- Creates new free block with remaining space

### Boundary-Tag Coalescing
- Forward coalescing: merges with next free block
- Backward coalescing: merges with previous free block
- Uses doubly-linked list for O(1) neighbor access
- Significantly reduces external fragmentation

## Implementation Details

### Data Structure

```c
struct block_meta {
  size_t size;              // Aligned size of user data
  struct block_meta *next;  // Next block pointer
  struct block_meta *prev;  // Previous block pointer
  int free;                 // Free status (0=allocated, 1=free)
  int magic;                // Validation magic number
  char padding[8];          // Cache-line alignment padding
};
```

### Configuration

- **Alignment**: 16 bytes (`ALIGNMENT`)
- **Coalesce Threshold**: 10 free operations (`COALESCE_THRESHOLD`)
- **Metadata Size**: `sizeof(struct block_meta)` (~48 bytes)

### Magic Numbers

- `0x12345678` - New allocation via `sbrk`
- `0x77777777` - Reused from free list
- `0x55555555` - Free block marker

## Building

### Compile the allocator

```bash
gcc -Wall -Wextra -pthread -c malloc.c -o malloc.o
```

### Build with test program

```bash
gcc -pthread test.c malloc.c -o test
./test
```

## Usage Example

```c
#include <stdio.h>
#include <string.h>

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nelem, size_t elsize);

int main(void) {
    // Allocate memory
    char *str = malloc(64);
    strcpy(str, "Custom allocator");
    
    // Resize allocation
    str = realloc(str, 128);
    strcat(str, " - expanded!");
    
    // Allocate zeroed array
    int *arr = calloc(10, sizeof(int));
    
    // Free memory
    free(str);
    free(arr);
    
    return 0;
}
```

## Thread Safety Test

```c
#include <pthread.h>

void *thread_func(void *arg) {
    for (int i = 0; i < 1000; i++) {
        void *p = malloc(128);
        // Use memory
        free(p);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[4];
    
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_func, NULL);
    }
    
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return 0;
}
```

## How It Works

### Allocation (`malloc`)
1. Lock mutex for thread safety
2. Check if deferred coalescing threshold reached
3. Align requested size to 16 bytes
4. Search free list using first-fit strategy
5. Split block if larger than needed
6. Request new heap space via `sbrk` if no suitable block found
7. Return aligned user pointer

### Deallocation (`free`)
1. Lock mutex
2. Validate pointer using magic number
3. Mark block as free
4. Increment free counter (deferred coalescing)
5. Unlock mutex
6. Actual coalescing happens during next malloc when threshold reached

### Deferred Coalescing
- Triggered in `malloc` when `free_count >= COALESCE_THRESHOLD`
- Traverses entire free list
- Merges all adjacent free blocks in one pass
- Resets counter to 0

### Reallocation (`realloc`)
1. If existing block large enough, return same pointer
2. Otherwise, allocate new aligned block
3. Copy data from old to new location
4. Free old block
5. Return new pointer

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| malloc | O(n) worst | First-fit traversal, O(1) with available free blocks |
| free | O(1) | Deferred coalescing |
| realloc | O(n) | May require new allocation + copy |
| calloc | O(n) | malloc + memset |
| Metadata access | O(1) | Pointer arithmetic |

## System Requirements

- Unix-like OS (Linux, macOS, BSD)
- POSIX threads support (pthread)
- GCC 4.8+ or Clang 3.5+
- System with `sbrk()` support

## Limitations

- Uses `sbrk()` which is deprecated on some modern systems
- Heap only grows, never shrinks (no memory unmapping)
- Single global mutex (could use finer-grained locking)
- No size-class segregation
- Not compatible with systems using address space layout randomization aggressively

## Technical Notes

- **First-fit strategy**: Balances speed vs fragmentation
- **Deferred coalescing**: Reduces overhead during rapid alloc/free patterns
- **16-byte alignment**: Optimizes for most CPU architectures
- **Doubly-linked list**: Enables bidirectional coalescing
- **Magic numbers**: Provides basic corruption detection via assertions

---

**Status**: Production-ready with full thread safety, alignment optimizations, and efficient fragmentation management.
