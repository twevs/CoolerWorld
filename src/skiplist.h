#pragma once

#include "common.h"
#include "arena.h"

/**
 * Paper: https://15721.courses.cs.cmu.edu/spring2019/papers/07-oltpindexes1/pugh-concurrent-tr1990.pdf
 *
 * This could form a good a basis for implementing an ordered map.
 */

#define MAX_LEVEL 10

struct SkipListNode
{
  SkipListNode **forwardPtrs;
  s32 height;
  f32 key;
  glm::vec3 value;
};

struct SkipList
{
  SkipListNode *header;
  s32 level;
};

SkipList CreateNewList(Arena *arena)
{
  SkipList list = {};
  list.header = (SkipListNode *)ArenaPush(arena, sizeof(SkipListNode));
  list.header->forwardPtrs = (SkipListNode **)ArenaPush(arena, sizeof(SkipListNode *) * MAX_LEVEL);
  list.header->height = MAX_LEVEL;
  list.level = 0;
  return list;
}

s32 RandomLevel()
{
  s32 level = 1;
  srand((u32)Win32GetWallClock());
  while (rand() < RAND_MAX / 2)
  {
    level++;
  }
  return min(level, MAX_LEVEL - 1);
}

SkipListNode *CreateNode(s32 level, f32 key, glm::vec3 value, Arena *arena)
{
  SkipListNode *result = (SkipListNode *)ArenaPush(arena, sizeof(SkipListNode));
  result->height = level;
  result->key = key;
  result->value = value;
  result->forwardPtrs = (SkipListNode **)ArenaPush(arena, sizeof(SkipListNode *) * level);
  return result;
}

void Insert(SkipList *list, f32 key, glm::vec3 value, Arena *arena, Arena *tempArena)
{
  SkipListNode **update = (SkipListNode **)tempArena->memory;
  
  SkipListNode *x = list->header;
  for (s32 i = list->level - 1; i >= 0; i--)
  {
    while (x->forwardPtrs[i] && x->forwardPtrs[i]->key > key)
    {
      x = x->forwardPtrs[i];
    }
    update[i] = *(SkipListNode **)ArenaPush(tempArena, sizeof(SkipListNode *));
    update[i] = x;
  }
  x = x->forwardPtrs[0];
  if (x && x->key == key)
  {
    x->value = value;
  }
  else
  {
    s32 level = RandomLevel();
    if (level > list->level)
    {
      for (s32 i = list->level; i < level; i++)
      {
        update[i] = list->header;
      }
      list->level = level;
    }
    x = CreateNode(level, key, value, arena);
    for (s32 i = 0; i < level; i++)
    {
      x->forwardPtrs[i] = update[i]->forwardPtrs[i];
      update[i]->forwardPtrs[i] = x;
    }
  }
}

glm::vec3 Search(SkipList *list, f32 key)
{
  SkipListNode *x = list->header;
  for (s32 i = list->level - 1; i >= 0; i--)
  {
    while (x->forwardPtrs[i] && x->forwardPtrs[i]->key > key)
    {
      x = x->forwardPtrs[i];
    }
  }
  x = x->forwardPtrs[0];
  if (x && x->key == key)
  {
    return x->value;
  }
  return glm::vec3();
}

f32 GetKey(SkipList *list, u32 index)
{
  u32 count = 0;
  SkipListNode *curNode = list->header->forwardPtrs[0];
  while (count++ < index)
  {
    curNode = curNode->forwardPtrs[0];
  }
  return curNode->key;
}

glm::vec3 GetValue(SkipList *list, u32 index)
{
  u32 count = 0;
  SkipListNode *curNode = list->header->forwardPtrs[0];
  while (count++ < index)
  {
    curNode = curNode->forwardPtrs[0];
  }
  return curNode->value;
}
