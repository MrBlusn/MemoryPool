#pragma once
#include "ThreadCache.h"

namespace MyMemoryPool
{

class MemoryPool
{
public:

    // 对外接口：分配和释放内存
    static void* allocate(size_t size)
    {
        return ThreadCache::getInstance()->allocate(size);
    }

    static void deallocate(void* ptr, size_t size)
    {
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
};

} // namespace MemoryPool