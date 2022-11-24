#pragma once

#include "common.h"

u64 fnv1a(u8 *data, size_t len)
{
    u64 hash = 14695981039346656037;
    u64 fnvPrime = 1099511628211;

    BYTE *currentByte = data;
    BYTE *end = data + len;
    while (currentByte < end)
    {
        hash ^= *currentByte;
        hash *= fnvPrime;
        ++currentByte;
    }

    return hash;
}

struct Arena
{
    void *memory;
    u64 stackPointer;
    u64 size;
};

internal Arena *AllocArena(u64 size)
{
    size_t allocated = sizeof(Arena) + size;
    void *mem = VirtualAlloc(NULL, allocated, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DebugPrintA("AllocArena(), %zu allocated\n", allocated);
    Arena *newArena = (Arena *)mem;
    newArena->memory = (u8 *)mem + sizeof(Arena);
    newArena->size = size;
    return newArena;
}

internal void FreeArena(Arena *arena)
{
    size_t deallocated = sizeof(Arena) + arena->size;
    VirtualFree(arena, deallocated, MEM_RELEASE);
    DebugPrintA("FreeArena(), %zu deallocated\n", deallocated);
}

internal void *ArenaPush(Arena *arena, u64 size)
{
    void *mem = (void *)((u8 *)arena->memory + arena->stackPointer);
    arena->stackPointer += size;
    myAssert(arena->stackPointer <= arena->size);
    return mem;
}

internal void ArenaPop(Arena *arena, u64 size)
{
    arena->stackPointer -= size;
    myAssert(arena->stackPointer >= 0);
}

internal void ArenaClear(Arena *arena)
{
    arena->stackPointer = 0;
}
