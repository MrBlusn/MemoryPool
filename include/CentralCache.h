#pragma once
#include "Common.h"
#include <mutex>
#include <unordered_map>
#include <array>
#include <atomic>
#include <chrono>

namespace MyMemoryPool
{

// 使用无锁的span信息存储
struct SpanTracker {
    std::atomic<void*> spanAddr{nullptr};   // 指向该 Span 的起始地址
    std::atomic<size_t> numPages{0};        // 占用多少个物理页（如 1 页 = 8KB）
    std::atomic<size_t> blockCount{0};      // 这个 Span 被切分成多少个小内存块
    std::atomic<size_t> freeCount{0}; // 用于追踪spn中还有多少块是空闲的，如果所有块都空闲，则归还span给PageCache
};

class CentralCache
{
public:
    static CentralCache& getInstance()      // 全局唯一实例,所有线程(整个进程)共享一个CentralCache对象
    {
        static CentralCache instance;
        return instance;
    }

    // 从 centralFreeList_[index] 中取出一整段连续的空闲内存块（比如 32 块），返回链表头。
    void* fetchRange(size_t index);
    // 将一批内存块（以 start 为头的链表）归还到 centralFreeList_[index]。
    void returnRange(void* start, size_t size, size_t index);

private:
    // 私有构造函数，禁止外部实例化
    CentralCache();
    // 从页缓存获取内存
    void* fetchFromPageCache(size_t size);

    // 获取span信息,根据一个小内存块的地址 blockAddr，快速找到它所属的 SpanTracker。
    SpanTracker* getSpanTracker(void* blockAddr);

    // 更新span的空闲计数并检查是否可以归还
    void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);

private:
    // 中心缓存的自由链表
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_{};        // 空闲内存块链的链首

    // 用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_{};                    // 每种内存块的门锁
    
    // 使用数组存储span信息，避免map的开销
    std::array<SpanTracker, 1024> spanTrackers_{};                            // 最多管理1024个内存页组
    std::atomic<size_t> spanCount_{0};                                      // 记录当前有多少个内存页组被管理

    // 延迟归还相关的成员变量
    static const size_t MAX_DELAY_COUNT = 48;  // 最大延迟计数
    std::array<std::atomic<size_t>, FREE_LIST_SIZE> delayCounts_{};  // 每个大小类的延迟计数
    std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> lastReturnTimes_{};  // 上次归还时间
    static const std::chrono::milliseconds DELAY_INTERVAL;  // 延迟间隔

    bool shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);
    void performDelayedReturn(size_t index);
};

} // namespace MemoryPool