#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

struct block_header
{
    struct block_header *next;
    size_t size;
    int isFree;
};

void *global_head = NULL;

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
    struct block_header *new_block;
    new_block = sbrk(0);
    void *request = sbrk(size + sizeof(struct block_header));
    if (request == (void *)-1)
    {
        return NULL;
    }
    if (last)
    {
        last->next = new_block;
    }
    new_block->size = size;
    new_block->next = NULL;
    new_block->isFree = 0;
    return new_block;
}

void *my_malloc(size_t size)
{
    if (size <= 0)
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
            block->isFree = 0;
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
        return NULL;
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
int main()
{
    clock_t start, end;

    start = clock();
    for (int i = 0; i < 100000; i++)
    {
        void *p = my_malloc(64);
        my_free(p);
    }
    end = clock();
    printf("my allocator: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    for (int i = 0; i < 100000; i++)
    {
        void *p = malloc(64);
        free(p);
    }
    end = clock();
    printf("glibc: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    return 0;
}