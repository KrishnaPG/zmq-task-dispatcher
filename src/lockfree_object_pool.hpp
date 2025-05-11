#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <stdexcept>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <optional>
#include <source_location>
#include <iostream>
#include <future>
#include <chrono>

// Enable PMR support if available
#define LFOP_USE_PMR 0

#if LFOP_USE_PMR
#include <memory_resource>
#endif

#if defined(_DEBUG)
#define LFOP_DEBUG 1
#else 
#undef LFOP_DEBUG
#endif

template <typename T>
class alignas(64) LockFreeObjectPool
{
    struct alignas(64) Node
    {
        std::atomic<Node*> next;
        alignas(T) std::byte storage[sizeof(T)];
    };

    // Configuration
    const size_t m_nMax_Thread_Cache;
    const size_t m_nBlockSize;

    // Free list
    std::atomic<Node*> m_pFreeList { nullptr };

    // Thread-local state
    struct ThreadCache
    {
        Node* cache = nullptr;
        size_t size = 0;
    };
    thread_local static inline std::unordered_map<const void*, ThreadCache> m_PerPoolCache; // per pool, per thread cache
    ThreadCache* m_pThreadCache = nullptr;

    // Scavenger thread (optional)
    std::jthread m_scavenger;

    // Reset hook
    using ResetHook = void(*)(T*);
    ResetHook m_fnResetHook = nullptr;

    // Memory resource (for PMR support)
#if LFOP_USE_PMR
    std::pmr::memory_resource* m_pmr;
#endif

    // Internal data
    std::vector<std::unique_ptr<Node[]>> m_vecPreAllocBlocks;

    // dynamic expantion
    const bool m_bDynamicExpansion;
    size_t m_nMaxTotalObjects; // optional max limit
    std::atomic<size_t> m_nCurrentTotalObjects { 0 };

public:
    void init_thread_cache()
    {
        if (!m_pThreadCache)
        {
            const void* poolId = this;
            auto [it, inserted] = m_PerPoolCache.try_emplace(poolId);
            m_pThreadCache = &it->second;
        }
    }

    explicit LockFreeObjectPool(size_t prealloc_count = 1024,
        size_t max_thread_cache = 32, 
        bool dynamic_expansion = true,
        size_t max_total_objects = SIZE_MAX,
        ResetHook reset_hook = nullptr
#if LFOP_USE_PMR
        , std::pmr::memory_resource* mr = std::pmr::get_default_resource()
#endif
    )
        : m_nMax_Thread_Cache(max_thread_cache),
        m_nBlockSize((prealloc_count + max_thread_cache - 1) / max_thread_cache + 1),
        m_bDynamicExpansion(dynamic_expansion),
        m_nMaxTotalObjects(max_total_objects),
        m_fnResetHook(reset_hook)
#if LFOP_USE_PMR
        , m_pmr(mr)
#endif
    {
        // Allocate memory using selected resource
        auto block = this->allocate_block(prealloc_count);
        m_vecPreAllocBlocks.push_back(std::move(block));

        // Build linked list
        Node* head = block.get();
        for (size_t i = 0; i < prealloc_count - 1; ++i)
        {
            block[i].next.store(&block[i + 1], std::memory_order_relaxed);
        }
        block[prealloc_count - 1].next.store(nullptr, std::memory_order_relaxed);

        // Push into global list
        Node* old_head = m_pFreeList.load(std::memory_order_relaxed);
        do
        {
            block[prealloc_count - 1].next.store(old_head, std::memory_order_relaxed);
        } while (!m_pFreeList.compare_exchange_weak(
            old_head, &block[0],
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    ~LockFreeObjectPool()
    {
        // 1. Signal shutdown
        this->m_bIsShuttingDown = true;

        // 2. Wait for all threads that are using this pool, to unregister.
        // When threads exit, the ~ThreadLocalPoolGuard()
        // automatically drains thread local cache into global
        wait_for_threads_shutdown();

        // 3. Stop scavenger
        if (m_scavenger.joinable())
        {
            m_scavenger.request_stop();
            m_scavenger.join();
        }

        // 4. Traverse and destroy global free list
        this->clear_global_list();

        // 5. Deallocate preallocated blocks
        for (auto& block : m_vecPreAllocBlocks)
        {
            for (size_t i = 0; i < m_nBlockSize; ++i)
            {
                Node& n = block[i];
                T* obj = reinterpret_cast<T*>(n.storage.data());
                if (reinterpret_cast<uintptr_t>(obj) & 1) continue; // skip already freed
                obj->~T();
                // Mark as freed
                new (&obj) T* { nullptr }; // poison pointer
            }
        }
    }

    template <typename... Args>
    T* acquire(Args&&... args)
    {
#ifdef LFOP_DEBUG
        if (m_bIsShuttingDown.load(std::memory_order_relaxed))
        {
            std::cerr << "[ERROR] Thread " << std::this_thread::get_id()
                << " tried to acquire from a destroyed pool.\n";
            assert(false && "LFOP Misuse detected");
        }
        if (!m_debugState.registered)
        {
            std::cerr << "[ERROR] Thread " << std::this_thread::get_id()
                << " used pool without registering via ThreadLocalPoolGuard.\n";
            assert(false && "LFOP Misuse detected");
        }
#endif
        Node* node = pop_cache();
        if (!node) node = pop_global();

        // allocate dynamically
        if (!node && m_bDynamicExpansion)
        {
            if (m_nCurrentTotalObjects.load(std::memory_order_relaxed) < m_nMaxTotalObjects)
            {
                node = allocate_new_node();
                if (node)
                {
                    ++m_nCurrentTotalObjects;
                }
            }
        }

        if (!node) [[unlikely]] {
            throw std::bad_alloc();
        }

        try
        {
            new (reinterpret_cast<T*>(node->storage.data())) T(std::forward<Args>(args)...);
        }
        catch (...)
        {
            push_global(node);
            throw;
        }

        return reinterpret_cast<T*>(node->storage.data());
    }

    void release(T* obj) noexcept
    {
        if (!obj) return;

        Node* node = reinterpret_cast<Node*>(
            reinterpret_cast<uintptr_t>(obj) -
            offsetof(Node, storage));

        if (m_fnResetHook) m_fnResetHook(obj);
        obj->~T();

        push_cache(node);
    }

    void start_scavenger(size_t interval_ms = 1000)
    {
        m_scavenger = std::jthread([this, interval_ms](std::stop_token stoken)
            {
                while (!stoken.stop_requested())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                    move_thread_cache_to_global();
                }
            });
    }

    template <typename... Args>
    std::shared_ptr<T> acquire_shared(Args&&... args)
    {
        T* obj = acquire(std::forward<Args>(args)...);
        return std::shared_ptr<T>(obj, [this](T* ptr) { this->release(ptr); });
    }

private:
    Node* allocate_block(size_t count)
    {
#if LFOP_USE_PMR
        void* raw = m_pmr->allocate(count * sizeof(Node), alignof(Node));
        return new (raw) Node[count];
#else
        return new Node[count];
#endif
    }

    void deallocate_block(Node* ptr, size_t count)
    {
#if LFOP_USE_PMR
        m_pmr->deallocate(ptr, count * sizeof(Node), alignof(Node));
#else
        delete[] ptr;
#endif
    }

    Node* allocate_new_node()
    {
#if LFOP_USE_PMR
        void* raw = m_pmr->allocate(sizeof(Node), alignof(Node));
        return new (raw) Node();
#else
        return new Node();
#endif
    }

    void deallocate_node(Node* node) noexcept
    {
        if (!node) return;

#if LFOP_USE_PMR
        m_pmr->deallocate(
            reinterpret_cast<std::byte*>(node),
            sizeof(Node),
            alignof(Node)
        );
#else
        delete node;
#endif
    }

    Node* pop_cache() noexcept
    {
        if (m_pThreadCache->cache)
        {
            Node* node = m_pThreadCache->cache;
            m_pThreadCache->cache = node->next.load(std::memory_order_relaxed);
            --m_pThreadCache->size;
            return node;
        }
        return nullptr;
    }

    void push_cache(Node* node)
    {
        if (m_pThreadCache->size < m_nMax_Thread_Cache)
        {
            node->next.store(m_pThreadCache->cache, std::memory_order_relaxed);
            m_pThreadCache->cache = node;
            ++m_pThreadCache->size;
        }
        else
        {
            push_global(node);
        }
    }

    Node* pop_global() noexcept
    {
        Node* old_head = m_pFreeList.load(std::memory_order_acquire);
        while (old_head)
        {
            Node* new_head = old_head->next.load(std::memory_order_relaxed);
            if (m_pFreeList.compare_exchange_weak(
                old_head, new_head,
                std::memory_order_release,
                std::memory_order_acquire))
            {
                return old_head;
            }
        }
        return nullptr;
    }

    void push_global(Node* node) noexcept
    {
        Node* old_head;
        do
        {
            old_head = m_pFreeList.load(std::memory_order_relaxed);
            node->next.store(old_head, std::memory_order_relaxed);
        } while (!m_pFreeList.compare_exchange_weak(
            old_head, node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    void move_thread_cache_to_global()
    {
        Node* head = m_pThreadCache->cache;
        m_pThreadCache->cache = nullptr;
        m_pThreadCache->size = 0;

        while (head)
        {
            Node* next = head->next.load(std::memory_order_relaxed);
            push_global(head);
            head = next;
        }
    }

    void clear_global_list()
    {
        Node* node = m_pFreeList.exchange(nullptr, std::memory_order_acquire);
        while (node)
        {
            T* obj = reinterpret_cast<T*>(node->storage.data());
            obj->~T(); // Explicitly destroy object

            Node* next = node->next.load(std::memory_order_relaxed);

            // If this node was dynamically allocated, we need to deallocate it
            if (!is_from_preallocated_block(node))
            {
                deallocate_node(node);
            }

            node = next;
        }
    }

protected:
    bool is_from_preallocated_block(Node* node) const noexcept
    {
        uintptr_t node_addr = reinterpret_cast<uintptr_t>(node);
        for (const auto& block : m_vecPreAllocBlocks)
        {
            uintptr_t block_start = reinterpret_cast<uintptr_t>(block.get());
            uintptr_t block_end = block_start + (sizeof(Node) * block_size_);
            if (node_addr >= block_start && node_addr < block_end)
            {
                return true;
            }
        }
        return false;
    }

    // helper mechanism for ThreadLocalPoolGuard
private:
    std::atomic<bool> m_bIsShuttingDown { false };
    std::atomic<size_t> m_nActiveThreads { 0 };
    std::mutex m_mutex_Shutdown;
    std::condition_variable m_cv_Shutdown;
#ifdef LFOP_DEBUG
    thread_local static struct ThreadLocalDebug
    {
        bool registered = false;
        bool used_after_shutdown = false;
    } m_debugState;
#endif
public:
    void register_thread()
    {
#ifdef LFOP_DEBUG
        if (m_bIsShuttingDown.load(std::memory_order_relaxed))
        {
            std::cerr << "[ERROR] Thread " << std::this_thread::get_id()
                << " tried to register after pool shutdown!\n";
            assert(false && "LFOP Misuse detected");
        }
        auto tid = std::this_thread::get_id();
        if (m_debugState.registered)
        {
            std::cerr << "[ERROR] Thread " << tid
                << " tried to double-register with the pool!\n";
            assert(false && "LFOP Misuse detected");
        }
        m_debugState.registered = true;
#endif
        ++m_nActiveThreads;
    }

    void unregister_thread()
    {
#ifdef LFOP_DEBUG
        auto tid = std::this_thread::get_id();
        if (!m_debugState.registered)
        {
            std::cerr << "[ERROR] Thread " << tid
                << " called unregister_thread() without registering.\n";
            assert(false && "LFOP Misuse detected");
        }
        m_debugState.registered = false;
#endif
        const size_t prev = m_nActiveThreads.fetch_sub(1, std::memory_order_acq_rel);
        if (m_bIsShuttingDown && prev == 1)
        {
            std::unique_lock<std::mutex> lock(m_mutex_Shutdown);
            m_cv_Shutdown.notify_all();
        }
    }

    void wait_for_threads_shutdown()
    {
        std::unique_lock<std::mutex> lock(m_mutex_Shutdown);
        m_cv_Shutdown.wait(lock, [this] { return m_nActiveThreads == 0; });
    }
};

/**
* ThreadLocalPoolGuard ensures all the thread local cache objects are 
* cleared per thread, since the ~LockFreeObjectPool() cannot automatically
* destroy thread local caches from all threads.
* @example:
    struct MyObj {
        int id;
        MyObj(int i) : id(i) {}
    };

    LockFreeObjectPool<MyObj> pool(1024);

    void worker() {
        ThreadLocalPoolGuard guard(pool); // Automatically drains on exit

        auto obj = pool.acquire_shared(42);
        std::cout << "Worker got object: " << obj->id << "\n";
    }
    int main() {
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back(worker);
        }

        for (auto& t : workers) {
            t.join();
        }

        // At this point, all threads have exited,
        // and ~LockFreeObjectPool() will safely destroy everything
        return 0;
    }
*/
template<typename T>
class ThreadLocalPoolGuard
{
    LockFreeObjectPool<T>& m_lfPool;
public:
    explicit ThreadLocalPoolGuard(LockFreeObjectPool<T>& pool)
        : m_lfPool(pool)
    {
        m_lfPool.register_thread();
        m_lfPool.init_thread_cache();
    }

    ~ThreadLocalPoolGuard()
    {
        m_lfPool.move_thread_cache_to_global();
        m_lfPool.unregister_thread();
    }
};