# MemoryPool

一个高性能 C++ 内存池实现，采用 **ThreadCache → CentralCache → PageCache** 三层缓存架构，通过线程本地存储（TLS）实现无锁快速路径，显著减少多线程场景下的内存分配开销。

## 架构概览

```
┌──────────────────────────────────────────────────┐
│                   MemoryPool                      │
│               (对外统一接口)                         │
└──────────────┬───────────────────────────────────┘
               │
┌──────────────▼───────────────────────────────────┐
│              ThreadCache (TLS)                     │
│  · 每个线程独立实例，无锁访问                         │
│  · 数组式自由链表，O(1) 分配/释放                    │
│  · 批量向 CentralCache 申请/归还内存                 │
└──────────────┬───────────────────────────────────┘
               │
┌──────────────▼───────────────────────────────────┐
│           CentralCache (全局单例)                   │
│  · 自旋锁保护，多线程安全                            │
│  · 按大小类切分内存块并分发给 ThreadCache             │
│  · 延迟归还 + Span 引用计数管理                      │
│  · 空闲 Span 自动回收至 PageCache                    │
└──────────────┬───────────────────────────────────┘
               │
┌──────────────▼───────────────────────────────────┐
│            PageCache (全局单例)                     │
│  · 以 4KB 页为单位管理内存                          │
│  · 支持 Span 分裂与合并，减少碎片                    │
│  · 通过 VirtualAlloc / mmap 直接向 OS 申请          │
└──────────────────────────────────────────────────┘
```

## 特性

- **三层缓存架构**：ThreadCache → CentralCache → PageCache，逐级向上申请，向下归还
- **线程本地无锁分配**：`thread_local` 的 ThreadCache 使常规分配无需加锁，O(1) 复杂度
- **大小类管理**：8 字节对齐，支持 8B ~ 256KB 的内存请求，通过 `SizeClass` 映射到固定索引
- **批量操作**：ThreadCache 批量从 CentralCache 获取/归还内存块，减少跨层交互频率
- **延迟归还策略**：基于计数 + 时间双重条件触发，避免频繁归还导致的锁竞争
- **Span 管理**：支持相邻 Span 合并，有效减少内存碎片
- **大对象直通**：超过 256KB 的请求直接走 `malloc`/`free`，不影响内存池效率
- **跨平台设计**：核心逻辑平台无关，OS 交互层可切换（当前 Windows `VirtualAlloc`）

## 目录结构

```
MemoryPool/
├── include/
│   ├── Common.h          # 公共定义：对齐、大小类、BlockHeader
│   ├── ThreadCache.h      # 线程本地缓存
│   ├── CentralCache.h     # 中心缓存（全局共享）
│   ├── PageCache.h        # 页缓存（OS 交互层）
│   └── MemoryPool.h       # 对外统一接口
├── src/
│   ├── ThreadCache.cpp
│   ├── CentralCache.cpp
│   └── PageCache.cpp
├── Tests/
│   ├── Unit.cpp           # 单元测试
│   └── PerformanceTest.cpp # 性能对比测试
└── README.md
```

## 快速开始

### 编译

```bash
# 编译单元测试
g++ -std=c++17 -O2 -pthread Tests/Unit.cpp src/ThreadCache.cpp src/CentralCache.cpp src/PageCache.cpp -o unit_test

# 编译性能测试
g++ -std=c++17 -O2 -pthread Tests/PerformanceTest.cpp src/ThreadCache.cpp src/CentralCache.cpp src/PageCache.cpp -o perf_test
```

### 使用示例

```cpp
#include "MemoryPool.h"

using namespace MyMemoryPool;

// 分配
void* ptr = MemoryPool::allocate(128);

// 使用...

// 释放（需传入分配时的大小）
MemoryPool::deallocate(ptr, 128);
```

### 运行测试

```bash
# 单元测试
./unit_test

# 性能对比测试（vs new/delete）
./perf_test
```

## 核心设计

### 大小类映射（SizeClass）

| 请求大小 | 对齐后 | 索引 |
|---------|--------|------|
| 1 ~ 8   | 8      | 0    |
| 9 ~ 16  | 16     | 1    |
| 17 ~ 24 | 24     | 2    |
| ...     | ...    | ...  |
| 256KB   | 256KB  | 32767|

### 分配流程

1. `MemoryPool::allocate(size)` → 路由到当前线程的 `ThreadCache`
2. ThreadCache 查找对应大小类的自由链表：
   - **命中**：弹出链表头，O(1) 返回
   - **未命中**：向 CentralCache 批量申请
3. CentralCache 从空闲链表取货，若不足则向 PageCache 申请新 Span 并切分
4. 超大请求（>256KB）直接走系统 `malloc`

### 归还流程

1. ThreadCache 插入回自由链表，达到阈值触发批量归还
2. CentralCache 接收归还块，通过延迟策略决定是否进一步归还给 PageCache
3. PageCache 回收完整 Span 并尝试与相邻 Span 合并

## 测试

### 单元测试覆盖
- 基础分配/释放
- 内存读写正确性
- 多线程安全（4 线程 × 1000 次随机分配释放）
- 边界条件（0 大小、最小对齐、最大边界）
- 压力测试（10000 次分配 + 乱序释放）

### 性能测试
- 小对象分配对比（vs `new`/`delete`）
- 多线程分配吞吐量
- 混合大小分配场景

## 适用场景

- 游戏服务器（频繁小对象分配）
- 高频交易系统（低延迟要求）
- 网络服务（高并发连接处理）
- 学习内存分配器设计原理

## 技术栈

- **语言**：C++17
- **同步**：`std::atomic`、自旋锁（`std::atomic_flag`）、`std::mutex`
- **线程**：`thread_local`（C++11）、`std::thread`
- **内存**：Windows `VirtualAlloc`（可替换为 `mmap`）

## License

MIT License

