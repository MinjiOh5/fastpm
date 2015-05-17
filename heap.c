#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include "heap.h"
#include "msg.h"

static char * BASE;
static char * FREE;
static size_t total_bytes;
static size_t used_bytes;

static struct Node {
    void * ptr;
    size_t bytes;
} NODES[1024];

static int top;

static size_t max_usage = 0;

int heap_init(size_t allocation) {
    BASE = malloc(allocation);
    total_bytes = allocation;
    if(BASE == NULL) {
        msg_abort(9999, "Failed to reserve %td bytes\n", allocation);
    } 
    FREE = BASE;
    used_bytes = 0;
    top = 0;
    return 0;
}

void * heap_allocate(size_t bytes) {
    /* align to 4K boundary */
    bytes = (bytes + 4096 - 1) / 4096;
    if(used_bytes + bytes < total_bytes) {
        void * ptr = FREE;
        FREE += bytes;
        used_bytes += bytes;
        if(used_bytes > max_usage) {
            max_usage = used_bytes;
        }
        NODES[top].ptr = ptr;
        NODES[top].bytes = bytes;
        top ++;
        return ptr;
    } else {
        msg_abort(9999, "Failed to allocation %td bytes\n", bytes);
        return NULL;
    }
}

size_t heap_get_max_usage() {
    return max_usage;
}

int heap_return(void * ptr) {
    top --;
    if (NODES[top].ptr != ptr) {
        msg_abort(9999, "Ptr %p is not last allocated block (%p of size %td)\n", 
                ptr, NODES[top].ptr, NODES[top].bytes);
        size_t bytes = NODES[top].bytes;
        FREE -= bytes;
        used_bytes -= bytes;
    }
    return 0;
}
