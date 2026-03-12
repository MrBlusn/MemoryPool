#pragma once
#include "Common.h" // 包含公共定义和工具函数

namespace MyMemoryPool 
{

// 线程本地缓存
class ThreadCache   // 封装线程本地缓存的所有逻辑，对外只暴露allocate/deallocate（分配 / 释放）接口，内部细节私有化。
{
public:
    // 单例模式和TLS
    static ThreadCache* getInstance()
    {
        static thread_local ThreadCache instance;   // thread_local（C++11）修饰的静态变量，保证每个线程有且仅有一个 ThreadCache 实例，且实例的生命周期与线程一致。
        return &instance;
    }

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
private:
    ThreadCache() 
    {
        // 初始化自由链表和大小统计
        freeList_.fill(nullptr);
        freeListSize_.fill(0);
    }
    
    // 从中心缓存获取内存
    void* fetchFromCentralCache(size_t index);
    // 归还内存到中心缓存
    void returnToCentralCache(void* start, size_t size);

    bool shouldReturnToCentralCache(size_t index);
private:
    // 每个线程的自由链表数组
    std::array<void*, FREE_LIST_SIZE>  freeList_; 
    std::array<size_t, FREE_LIST_SIZE> freeListSize_; // 自由链表大小统计   
};

} // namespace memoryPool