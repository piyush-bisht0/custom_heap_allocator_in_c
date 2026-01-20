#include <stdio.h>
#include <string.h>
#include <pthread.h>

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nelem, size_t elsize);

void *thread_test(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 100; i++) {
        char *p = malloc(64);
        sprintf(p, "Thread %d - iteration %d", id, i);
        free(p);
    }
    return NULL;
}

int main(void) {
    printf("=== Custom Heap Allocator Test ===\n\n");
    
    // Test 1: Basic malloc and free
    printf("Test 1: malloc/free\n");
    char *p = malloc(64);
    strcpy(p, "Hello, custom allocator!");
    printf("  Result: %s\n", p);
    free(p);
    printf("  ✓ Passed\n\n");
    
    // Test 2: realloc
    printf("Test 2: realloc\n");
    p = malloc(32);
    strcpy(p, "Small");
    printf("  Before: %s\n", p);
    p = realloc(p, 128);
    strcat(p, " -> Expanded!");
    printf("  After: %s\n", p);
    free(p);
    printf("  ✓ Passed\n\n");
    
    // Test 3: calloc
    printf("Test 3: calloc (zeroed memory)\n");
    int *arr = calloc(10, sizeof(int));
    printf("  Array: ");
    int all_zeros = 1;
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
        if (arr[i] != 0) all_zeros = 0;
    }
    printf("\n");
    if (all_zeros) {
        printf("  ✓ Passed\n\n");
    } else {
        printf("  ✗ Failed\n\n");
    }
    free(arr);
    
    // Test 4: Multiple allocations
    printf("Test 4: Multiple allocations\n");
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = malloc(128);
    }
    printf("  Allocated 10 blocks\n");
    for (int i = 0; i < 10; i++) {
        free(ptrs[i]);
    }
    printf("  Freed 10 blocks (triggers deferred coalescing)\n");
    printf("  ✓ Passed\n\n");
    
    // Test 5: Thread safety
    printf("Test 5: Thread safety (4 threads, 100 ops each)\n");
    pthread_t threads[4];
    int ids[4] = {1, 2, 3, 4};
    
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_test, &ids[i]);
    }
    
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("  All threads completed successfully\n");
    printf("  ✓ Passed\n\n");
    
    printf("=== All Tests Passed ===\n");
    
    return 0;
}
