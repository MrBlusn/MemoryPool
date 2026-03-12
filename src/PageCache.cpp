#include "PageCache.h"
#include <Windows.h>
#include <cstring>

namespace MyMemoryPool
{

void* PageCache::allocateSpan(size_t numPages)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找合适的空闲span
    // lower_bound函数返回第一个大于等于numPages的元素的迭代器
    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end())
    {
        Span* span = it->second;

        // 将取出的span从原有的空闲链表freeSpans_[it->first]中移除
        if (span->next)
        {
            freeSpans_[it->first] = span->next;
        }
        else
        {
            freeSpans_.erase(it);
        }

        // 如果span大于需要的numPages则进行分割
        if (span->numPages > numPages) 
        {
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + 
                                numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;

            // 将超出部分放回空闲Span*列表头部
            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;

            span->numPages = numPages;
        }

        // 记录span信息用于回收
        spanMap_[span->pageAddr] = span;
        return span->pageAddr;
    }

    // 没有合适的span，向系统申请
    void* memory = systemAlloc(numPages);
    if (!memory) return nullptr;

    // 创建新的span
    Span* span = new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    // 记录span信息用于回收
    spanMap_[memory] = span;
    return memory;
}

void PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) return;

    Span* span = it->second;

    // 尝试合并相邻的span
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = spanMap_.find(nextAddr);
    
    if (nextIt != spanMap_.end())
    {
        Span* nextSpan = nextIt->second;
        
        // 1. 首先检查nextSpan是否在空闲链表中
        bool found = false;
        auto& nextList = freeSpans_[nextSpan->numPages];
        
        // 检查是否是头节点
        if (nextList == nextSpan)
        {
            nextList = nextSpan->next;
            found = true;
        }
        else if (nextList) // 只有在链表非空时才遍历
        {
            Span* prev = nextList;
            while (prev->next)
            {
                if (prev->next == nextSpan)
                {   
                    // 将nextSpan从空闲链表中移除
                    prev->next = nextSpan->next;
                    found = true;
                    break;
                }
                prev = prev->next;
            }
        }

        // 2. 只有在找到nextSpan的情况下才进行合并
        if (found)
        {
            // 合并span
            span->numPages += nextSpan->numPages;
            spanMap_.erase(nextAddr);
            delete nextSpan;
        }
    }

    // 将合并后的span通过头插法插入空闲列表
    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;
}

void* PageCache::systemAlloc(size_t numPages)
{
    // 1. 计算需要分配的总字节数（页数 × 每页4096字节）
    size_t size = numPages * PAGE_SIZE;

    // 2. Windows下用VirtualAlloc分配虚拟内存（替代Linux的mmap）
    // 参数说明（零基础版）：
    // - NULL：让系统自动选择内存起始地址
    // - size：要分配的字节数
    // - MEM_COMMIT | MEM_RESERVE：先保留地址空间，再提交物理内存（对应Linux的MAP_ANONYMOUS）
    // - PAGE_READWRITE：内存权限为可读可写（对应Linux的PROT_READ | PROT_WRITE）
    void* ptr = VirtualAlloc(
        NULL,               // 自动分配地址
        size,               // 分配的字节数
        MEM_COMMIT | MEM_RESERVE,  // 分配类型（提交+保留）
        PAGE_READWRITE      // 内存保护属性（可读可写）
    );

    // 3. 检查分配是否失败（Windows失败返回NULL，而非MAP_FAILED）
    if (ptr == NULL)
    {
        return nullptr;
    }

    // 4. 清零内存（和原逻辑一致，避免脏数据）
    memset(ptr, 0, size);
    return ptr;
}

} // namespace memoryPool