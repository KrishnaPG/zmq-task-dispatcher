#pragma once

template<typename T>
class MpscQueue
{
    struct Node
    {
        std::atomic<Node*> next = nullptr;
        T data;
    };

    alignas(64) std::atomic<Node*> m_pHead;
    alignas(64) Node* m_pTail;  // Only accessed by consumer (main thread)
    std::atomic<size_t> m_nSize { 0 };

public:
    MpscQueue() : m_pHead(new Node), m_pTail(m_pHead.load()) { }
    ~MpscQueue() { while (pop()); } // Drain remaining messages

    // Push from any thread (producer)
    void push(T&& item)
    {
        auto* new_node = new Node { std::move(item), nullptr };
        Node* prev_head = m_pHead.exchange(new_node, std::memory_order_acq_rel);
        prev_head->next.store(new_node, std::memory_order_release);
        m_nSize.fetch_add(1, std::memory_order_relaxed);
    }

    // Pop only from consumer thread (main thread)
    std::optional<T> pop()
    {
        Node* first = m_pTail->next.load(std::memory_order_acquire);
        if (!first) return std::nullopt;

        T data = std::move(first->data);
        delete std::exchange(m_pTail, first);
        m_nSize.fetch_sub(1, std::memory_order_relaxed);
        return data;
    }

    size_t size() const noexcept
    {
        return m_nSize.load(std::memory_order_relaxed);
    }
};