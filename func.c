#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

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

void free(void *pointer)
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

void *realloc(void *pointer, size_t size)
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
    new_pointer = malloc(size);
    if (!new_pointer)
    {
        return NULL;
    }
    memcpy(new_pointer, pointer, block_data->size);
    free(pointer);
    return new_pointer;
}

void *calloc(size_t n, size_t esize)
{
    if (esize && n > SIZE_MAX / esize)
    {
        return NULL;
    }
    size_t size = n * esize;
    void *new_pointer = malloc(size);
    if (!new_pointer)
    {
        return NULL;
    }
    memset(new_pointer, 0, size);
    return new_pointer;
}

int main()
{
    int *a = malloc(sizeof(int));
    *a = 1;
    assert(*a == 1);

    free(a);
    int *b = malloc(sizeof(int));
    assert(b == a);

    int *c = malloc(sizeof(int) * 4);
    int *d = realloc(c, sizeof(int) * 5); // change to 3 as well
    assert(d != NULL);

    int *e = calloc(4, sizeof(int));
    for (int i = 0; i < 4; i++)
    {
        assert(e[i] == 0);
    }

    int *x = malloc(16);
    int *y = malloc(32);
    free(x);
    free(y);
    int *z = malloc(48);
    assert(z == x);

    // Checks all functionality
    printf("All tests cleared\n");
    return 0;
}