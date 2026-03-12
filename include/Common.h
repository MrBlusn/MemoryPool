#pragma once    //头文件保护，防止重复包含
#include <cstddef> // 包含size_t定义
#include <atomic>  // 预留未来可能使用的原子操作
#include <array>    // 预留未来可能使用的数组容器
#include <cstdlib>   // 包含malloc/free定义

namespace MyMemoryPool 
{
// 对齐数和大小定义
constexpr size_t ALIGNMENT = 8; //对其的字节数，通常为8字节
constexpr size_t MAX_BYTES = 256 * 1024; // 256KB，内存池支持的最大内存块
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT等于指针void*的大小, 在64位系统上为8字节

// 内存块头部信息
struct BlockHeader  // 定义内存块的元数据
{
    size_t size; // 内存块大小
    bool   inUse; // 使用标志
    BlockHeader* next; // 指向下一个内存块
};

// 大小类管理
class SizeClass 
{
public:
    // 将输入的bytes向上对齐到ALIGNMENT的整数倍（核心算法）
    static size_t roundUp(size_t bytes)
    {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    // 将「任意大小的内存请求」映射到「固定的自由链表索引」，比如用户申请 10 字节，最终分配 16 字节，对应索引 1 的链表
    static size_t getIndex(size_t bytes)
    {   
        // 确保bytes至少为ALIGNMENT
        bytes = std::max(bytes, ALIGNMENT);
        // 向上取整后-1
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
};

} // namespace memoryPool