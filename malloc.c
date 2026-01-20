#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define META_SIZE sizeof(struct block_meta)
#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define COALESCE_THRESHOLD 10

void *global_base = NULL;
pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;
int free_count = 0;

struct block_meta {
  size_t size;
  struct block_meta *next;
  struct block_meta *prev;
  int free;
  int magic;
  char padding[8];
};

struct block_meta *find_free_block(struct block_meta **last, size_t size) {
  struct block_meta *current = global_base;
  while (current && !(current->free && current->size >= size)) {
    *last = current;
    current = current->next;
  }
  return current;
}

struct block_meta *request_space(struct block_meta *last, size_t size) {
  struct block_meta *block;
  size_t aligned_size = ALIGN(size);
  
  block = sbrk(0);
  void *request = sbrk(aligned_size + META_SIZE);
  assert((void *)block == request);
  if (request == (void *)-1) {
    return NULL;
  }

  if (last) {
    last->next = block;
    block->prev = last;
  } else {
    block->prev = NULL;
  }
  block->size = aligned_size;
  block->next = NULL;
  block->free = 0;
  block->magic = 0x12345678;
  return block;
}

struct block_meta *get_block_ptr(void *ptr) {
  return (struct block_meta *)ptr - 1;
}

void split_block(struct block_meta *block, size_t size) {
  size_t aligned_size = ALIGN(size);
  if (block->size >= aligned_size + META_SIZE + ALIGNMENT) {
    struct block_meta *new_block =
        (struct block_meta *)((char *)block + META_SIZE + aligned_size);
    new_block->size = block->size - aligned_size - META_SIZE;
    new_block->free = 1;
    new_block->next = block->next;
    new_block->prev = block;
    new_block->magic = 0x55555555;

    if (new_block->next) {
      new_block->next->prev = new_block;
    }

    block->next = new_block;
    block->size = aligned_size;
  }
}

struct block_meta *coalesce(struct block_meta *block) {
  if (block->next && block->next->free) {
    block->size += META_SIZE + block->next->size;
    block->next = block->next->next;
    if (block->next) {
      block->next->prev = block;
    }
  }

  if (block->prev && block->prev->free) {
    block->prev->size += META_SIZE + block->size;
    block->prev->next = block->next;
    if (block->next) {
      block->next->prev = block->prev;
    }
    block = block->prev;
  }

  return block;
}

void deferred_coalesce() {
  struct block_meta *current = global_base;
  while (current) {
    if (current->free) {
      current = coalesce(current);
    }
    current = current->next;
  }
  free_count = 0;
}

void *malloc(size_t size) {
  struct block_meta *block;

  if (size <= 0) {
    return NULL;
  }

  pthread_mutex_lock(&malloc_mutex);

  if (free_count >= COALESCE_THRESHOLD) {
    deferred_coalesce();
  }

  size_t aligned_size = ALIGN(size);

  if (!global_base) {
    block = request_space(NULL, aligned_size);
    if (!block) {
      pthread_mutex_unlock(&malloc_mutex);
      return NULL;
    }
    global_base = block;
  } else {
    struct block_meta *last = global_base;
    block = find_free_block(&last, aligned_size);
    if (!block) {
      block = request_space(last, aligned_size);
      if (!block) {
        pthread_mutex_unlock(&malloc_mutex);
        return NULL;
      }
    } else {
      split_block(block, aligned_size);
      block->free = 0;
      block->magic = 0x77777777;
    }
  }

  pthread_mutex_unlock(&malloc_mutex);
  return (block + 1);
}

void free(void *ptr) {
  if (!ptr) {
    return;
  }

  pthread_mutex_lock(&malloc_mutex);

  struct block_meta *block_ptr = get_block_ptr(ptr);
  assert(block_ptr->free == 0);
  assert(block_ptr->magic == 0x77777777 || block_ptr->magic == 0x12345678);
  block_ptr->free = 1;
  block_ptr->magic = 0x55555555;
  
  free_count++;
  
  pthread_mutex_unlock(&malloc_mutex);
}

void *realloc(void *ptr, size_t size) {
  if (!ptr) {
    return malloc(size);
  }

  pthread_mutex_lock(&malloc_mutex);

  struct block_meta *block_ptr = get_block_ptr(ptr);
  size_t aligned_size = ALIGN(size);
  
  if (block_ptr->size >= aligned_size) {
    pthread_mutex_unlock(&malloc_mutex);
    return ptr;
  }

  pthread_mutex_unlock(&malloc_mutex);

  void *new_ptr;
  new_ptr = malloc(size);
  if (!new_ptr) {
    return NULL;
  }

  memcpy(new_ptr, ptr, block_ptr->size);
  free(ptr);
  return new_ptr;
}

void *calloc(size_t nelem, size_t elsize) {
  size_t size = nelem * elsize;
  void *ptr = malloc(size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}
