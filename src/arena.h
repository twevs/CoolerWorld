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
    void *mem = VirtualAlloc(NULL, sizeof(Arena) + size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Arena *newArena = (Arena *)mem;
    newArena->memory = (u8 *)mem + sizeof(Arena);
    newArena->size = size;
    return newArena;
}

internal void FreeArena(Arena *arena)
{
    VirtualFree(arena, arena->size, MEM_RELEASE);
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

