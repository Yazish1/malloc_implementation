#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

struct block_header
{
    struct block_header *next;
    size_t size;
    int isFree;
    char padding[12];
};

struct block_header *global_head = NULL;

void split_block(struct block_header *block, size_t size)
{
    struct block_header *leftover = (struct block_header *)((char *)block + sizeof(struct block_header) + size);
    leftover->size = block->size - size - sizeof(struct block_header);
    leftover->next = block->next;
    block->next = leftover;
    leftover->isFree = 1;
    block->size = size;
    block->isFree = 0;
}
struct block_header *find_next_free(struct block_header **last, size_t size)
{
    struct block_header *current = global_head;
    while (current && !(current->size >= size && current->isFree))
    {
        *last = current;
        current = current->next;
    }
    return current;
}

struct block_header *request_extra(struct block_header *last, size_t size)
{
    size_t chunk = size + sizeof(struct block_header);
    if (chunk < 4096)
    {
        chunk = 4096;
    }

    struct block_header *new_block = mmap(NULL, chunk, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_block == MAP_FAILED)
    {
        return NULL;
    }
    if (last)
    {
        last->next = new_block;
    }
    new_block->size = chunk - sizeof(struct block_header);
    new_block->next = NULL;
    new_block->isFree = 0;
    if (new_block->size - size >= sizeof(struct block_header) + 1)
    {
        split_block(new_block, size);
    }
    return new_block;
}

void *my_malloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    struct block_header *block;
    if (!global_head)
    {
        block = request_extra(NULL, size);
        if (!block)
        {
            return NULL;
        }
        global_head = block;
    }
    else
    {
        struct block_header *last = global_head;
        block = find_next_free(&last, size);
        if (block == NULL)
        {
            block = request_extra(last, size);
        }
        else
        {
            if (block->size - size >= sizeof(struct block_header) + 1)
            {
                split_block(block, size);
            }
            else
            {
                block->isFree = 0;
            }
        }
    }
    return (block + 1);
}

void my_free(void *pointer)
{
    if (!pointer)
    {
        return;
    }
    struct block_header *block_data = (struct block_header *)pointer - 1;
    if (block_data->isFree == 1)
    {
        return;
    }
    block_data->isFree = 1;
    struct block_header *current = global_head;
    while (current && current->next)
    {
        if (current->isFree && current->next->isFree)
        {
            current->size += current->next->size + sizeof(struct block_header);
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }
}

void *my_realloc(void *pointer, size_t size)
{
    if (!pointer)
    {
        return my_malloc(size);
    }
    struct block_header *block_data = (struct block_header *)pointer - 1;
    if (block_data->size >= size)
    {
        return pointer;
    }
    void *new_pointer;
    new_pointer = my_malloc(size);
    if (!new_pointer)
    {
        return NULL;
    }
    memcpy(new_pointer, pointer, block_data->size);
    my_free(pointer);
    return new_pointer;
}

void *my_calloc(size_t n, size_t esize)
{
    if (esize && n > SIZE_MAX / esize)
    {
        return NULL;
    }
    size_t size = n * esize;
    void *new_pointer = my_malloc(size);
    if (!new_pointer)
    {
        return NULL;
    }
    memset(new_pointer, 0, size);
    return new_pointer;
}

#ifndef BUILD_LIB
int main()
{
    clock_t start, end;
    void *ptrs[10000];

    // CHURN TEST
    start = clock();
    for (int i = 0; i < 100000; i++)
    {
        void *p = my_malloc(64);
        my_free(p);
    }
    end = clock();
    printf("churn - my allocator: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    for (int i = 0; i < 100000; i++)
    {
        void *p = malloc(64);
        free(p);
    }
    end = clock();
    printf("churn - glibc:        %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    // RAMP TEST
    start = clock();
    for (int i = 0; i < 10000; i++)
        ptrs[i] = my_malloc(64);

    // measure fragmentation WHILE blocks are in use
    size_t total = 0, used = 0;
    struct block_header *cur = global_head;
    while (cur)
    {
        total += cur->size + sizeof(struct block_header);
        if (!cur->isFree)
            used += cur->size;
        cur = cur->next;
    }
    printf("fragmentation ratio: %.2f\n", (double)total / used);

    for (int i = 0; i < 10000; i++)
        my_free(ptrs[i]);
    end = clock();
    printf("ramp  - my allocator: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    for (int i = 0; i < 10000; i++)
        ptrs[i] = malloc(64);
    for (int i = 0; i < 10000; i++)
        free(ptrs[i]);
    end = clock();
    printf("ramp  - glibc:        %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    // MIXED SIZES
    start = clock();
    srand(42);
    for (int i = 0; i < 100000; i++)
    {
        void *p = my_malloc(rand() % 512 + 8);
        my_free(p);
    }
    end = clock();
    printf("mixed - my allocator: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    srand(42);
    for (int i = 0; i < 100000; i++)
    {
        void *p = malloc(rand() % 512 + 8);
        free(p);
    }
    end = clock();
    printf("mixed - glibc:        %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    return 0;
}
#endif