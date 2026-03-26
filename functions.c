#include <assert.h>
#include <stdio.h>
typedef unsigned int size_t;
struct block_header
{
    struct block_header *next;
    size_t size;
    int isFree;
};

#define HEADER_SIZE sizeof(struct block_header);
void *global_head = NULL;

struct block_header *find_next_free(struct block_header **last, size_t size)
{
    struct block_header *current = *last;
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
    void *request = sbrk(size);
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

void *malloc(size_t size)
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
        block = find_next_free(*last, size);
        if (block == NULL)
        {
            return NULL;
        }
        else
        {
            block->isFree = 0;
        }
    }
    return (block + 1);
}

void free(void *pointer)
{
    if (!pointer)
    {
        return;
    }
    struct block_header *block_data = (struct block_header *)(pointer - 1);
    if (block_data->isFree == 1)
    {
        return;
    }
    block_data->isFree = 1;
}

void realloc(void *pointer, size_t size)
{
    if (!pointer)
    {
        return;
    }
    struct block_header *block_data = (struct block_header *)(pointer - 1);
    if (block_data->size >= size)
    {
        return pointer;
    }
    void *new_pointer;
    new_pointer = malloc(size);
    if (!new_pointer)
    {
        return;
    }
    memcpy(new_pointer, pointer, block_data->size);
    free(pointer);
    return new_pointer;
}

void calloc(size_t n, size_t esize)
{
    size_t size = n * esize;
    void *new_pointer = malloc(size);
    if (!new_pointer)
    {
        return;
    }
    memset(new_pointer, 0, size);
    return new_pointer;
}
