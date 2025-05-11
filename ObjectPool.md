# Lockfree Object Pool with C++

- 📦 Overview
- ✨ Key Features
- 🧠 Design Highlights
- 🚀 Getting Started
- 🧪 Example Usage
- ⚙️ Configuration Options
- 🧼 Cleanup & Destruction Safety
- 🧰 Thread Coordination
- 📁 License

---

# High-Performance, Thread-Safe Object Pool in C++23

A modern, lock-free, thread-safe, and high-performance object pool with support for:
- Thread-local caching
- Global lock-free fallback stack
- Zero-copy semantics
- Shared ownership via `std::shared_ptr`
- Custom reset hooks
- Optional dynamic expansion
- PMR (C++17+) memory resource support
- Safe destruction across threads

---

## ✅ Key Features

| Feature | Description |
|--------|-------------|
| 🔒 Thread-safe | Uses atomic CAS operations |
| 🧠 Thread-local cache | Fast allocation per-thread |
| 🌐 Global lock-free list | Fallback when local cache is empty |
| 🧱 Preallocated memory | No runtime allocations after init |
| 🔄 Dynamic expansion | Optionally allocates more objects when exhausted |
| 🗑️ Manual destruction | Placement new + explicit destructor call |
| 🧩 Generic type support | Works with any movable `T` |
| 📦 `std::shared_ptr` support | Use `acquire_shared()` for shared ownership |
| 🛠️ Reset hooks | Optional custom logic before reuse |
| 🧯 Safe destruction | Waits for all threads to drain caches |
| 📏 Debug misuse detection | Detects double-unregister, use-after-shutdown |
| 🧵 Scavenger thread | Optional background cache-to-global cleanup |
| 🧱 PMR Support | Optional custom memory resource integration |

---

## 🚀 Getting Started

### Requirements

- **C++20 or higher**
- Optional: `std::pmr` support for custom allocators (C++17+)

### Build Instructions

Just include the header:

```cpp
#include "lockfree_object_pool.hpp"
```

Then instantiate a pool:

```cpp
LockFreeObjectPool<MyType> pool(1024); // preallocate 1024 objects
```

Use RAII guard to safely manage thread-local usage:

```cpp
ThreadLocalPoolGuard<MyType> guard(pool);
```

---

## 🧪 Example Usage

```cpp
struct MyObj {
    int id;
    MyObj(int i) : id(i) {}
};

void worker(LockFreeObjectPool<MyObj>& pool) {
    ThreadLocalPoolGuard<MyObj> guard(pool);

    auto obj = pool.acquire_shared(42);
    std::cout << "Worker got object: " << obj->id << "\n";
}

int main() {
    LockFreeObjectPool<MyObj> pool(1024);
    pool.start_scavenger(); // optional background cleanup

    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back(worker, std::ref(pool));
    }

    for (auto& t : workers) t.join();

    return 0;
}
```

---

## ⚙️ Configuration Options

You can customize your pool using constructor arguments:

```cpp
explicit LockFreeObjectPool(
    size_t prealloc_count = 1024,
    size_t max_thread_cache = 32,
    bool dynamic_expansion = true,
    size_t max_total_objects = SIZE_MAX,
    ResetHook reset_hook = nullptr
#if LFOP_USE_PMR
    , std::pmr::memory_resource* mr = std::pmr::get_default_resource()
#endif
)
```

| Parameter | Purpose |
|----------|---------|
| `prealloc_count` | Number of objects to preallocate at startup |
| `max_thread_cache` | Max number of cached nodes per thread |
| `dynamic_expansion` | Allow heap allocation if prealloc runs out |
| `max_total_objects` | Optional cap on total allocated objects |
| `reset_hook` | Optional function to reset object state before reuse |
| `mr` | Memory resource for PMR support |

---

## 🧼 Safe Destruction & Cleanup

The destructor waits for all registered threads to unregister before cleaning up.

To ensure safe shutdown:
- Threads must use `ThreadLocalPoolGuard`
- The guard ensures thread-local cache is drained before exit
- Destructor calls `wait_for_threads_shutdown()` internally

---

## 🧰 Thread Coordination

Each thread gets its own:
- Thread-local cache (`m_pThreadCache`)
- Registration tracking
- Misuse detection (via debug macros)

Use `register_thread()` / `unregister_thread()` internally.
RAII guard does this automatically.

---

## 🧹 Optional Cleanup Helpers

- `start_scavenger(interval_ms)` – Background thread moves local cache to global periodically
- `move_thread_cache_to_global()` – Manually move local cache to global
- `clear_global_list()` – Destroy and deallocate all objects from global list

---

## 📁 Supported Compiler Flags

| Flag | Effect |
|------|--------|
| `LFOP_DEBUG` | Enables misuse detection (e.g., use-after-shutdown, double-register) |
| `LFOP_USE_PMR` | Enables custom memory resource support |
| `_DEBUG` | Automatically enables `LFOP_DEBUG` |

---

## 🧊 Advanced Features

| Feature | Description |
|--------|-------------|
| 📈 Dynamic Expansion | Falls back to heap allocation if needed |
| 🧠 Per-Pool Thread Cache | Each pool has its own cache per thread |
| 🧭 Custom Deleter Support | `acquire_shared()` returns `shared_ptr` that releases to pool |
| 🧷 Reset Hook Support | Inject custom logic before reuse |
| 🧬 Zero-Copy Reuse | Placement new + manual destructor call |
| 🧮 Statistics Tracking | Extendable with counters/metrics |

---

## 📜 License

MIT License — Free for commercial and personal use.

---
