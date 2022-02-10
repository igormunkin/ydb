#pragma once

#include <atomic>

namespace NKikimr {
namespace NUtil {

    /**
     * Concurrent single-writer multiple-readers vector-like store with stable element pointers
     */
    template<class T>
    class TConcurrentStore {
    private:
        struct TChunk {
            const size_t Offset;
            const size_t Bytes;
            TChunk* const Prev;
            std::atomic<TChunk*> Next;

            TChunk(size_t offset, size_t bytes, TChunk* prev)
                : Offset(offset)
                , Bytes(bytes)
                , Prev(prev)
                , Next{ nullptr }
            { }

            static constexpr size_t ValuesOffset() {
                size_t size = sizeof(TChunk);
                size += (-size) & (alignof(T) - 1);
                return size;
            }

            static constexpr size_t MinBytes() {
                return ValuesOffset() + sizeof(T);
            }

            size_t Capacity() const {
                return (Bytes - ValuesOffset()) / sizeof(T);
            }

            size_t EndOffset() const {
                return Offset + Capacity();
            }

            T* Values() {
                char* base = reinterpret_cast<char*>(this);
                return reinterpret_cast<T*>(base + ValuesOffset());
            }

            const T* Values() const {
                const char* base = reinterpret_cast<const char*>(this);
                return reinterpret_cast<const T*>(base + ValuesOffset());
            }
        };

    public:
        class TConstIterator {
        public:
            explicit TConstIterator(const TConcurrentStore* store) noexcept
                : Offset(0)
                , Count(store->Count.load(std::memory_order_acquire))
                , Head(store->Head.load(std::memory_order_acquire))
            { }

            bool IsValid() const noexcept {
                return Offset < Count && Head;
            }

            bool Next() noexcept {
                if (++Offset >= Count) {
                    Head = nullptr;
                    return false;
                }

                if (Head && Head->EndOffset() <= Offset) {
                    Head = Head->Next.load(std::memory_order_acquire);
                }

                Y_VERIFY_DEBUG(Head && Head->Offset <= Offset && Offset < Head->EndOffset(),
                    "Unexpected failure to find chunk for offset %" PRISZT, Offset);

                return true;
            }

            const T* operator->() const noexcept {
                return GetPtr();
            }

            const T& operator*() const noexcept {
                return *GetPtr();
            }

        private:
            const T* GetPtr() const noexcept {
                Y_VERIFY_DEBUG(IsValid());
                Y_VERIFY_DEBUG(Head->Offset <= Offset && Offset < Head->EndOffset());

                return Head->Values() + (Offset - Head->Offset);
            }

        private:
            size_t Offset;
            size_t Count;
            TChunk* Head;
        };

    public:
        TConcurrentStore() { }

        ~TConcurrentStore() noexcept {
            clear();
        }

        TConstIterator Iterator() const noexcept {
            return TConstIterator(this);
        }

        void clear() noexcept {
            size_t count = Count.exchange(0, std::memory_order_release);
            Head.store(nullptr, std::memory_order_release);
            auto* tail = Tail.exchange(nullptr, std::memory_order_release);
            while (tail) {
                if (tail->Offset < count) {
                    size_t elements = std::min(count - tail->Offset, tail->Capacity());
                    T* values = tail->Values();
                    for (size_t i = 0; i < elements; ++i, ++values) {
                        values->~T();
                    }
                }
                auto* prev = tail->Prev;
                FreeChunk(tail);
                tail = prev;
            }
        }

        /**
         * Emplaces a new element, not thread safe
         */
        template<class... TArgs>
        T& emplace_back(TArgs&&... args) {
            // Claim index for the new item
            size_t index = Count.load(std::memory_order_relaxed);

            // Allocate a new chunk if necessary
            auto* tail = Tail.load(std::memory_order_relaxed);
            if (!tail || tail->EndOffset() <= index) {
                size_t offset = tail ? tail->EndOffset() : 0;
                size_t bytes = tail ? tail->Bytes * 2 : 512;
                if (bytes < TChunk::MinBytes()) {
                    bytes = TChunk::MinBytes();
                }
                tail = AllocateChunk(offset, bytes, tail);
                if (tail->Prev) {
                    tail->Prev->Next.store(tail, std::memory_order_release);
                } else {
                    Head.store(tail, std::memory_order_release);
                }
                Tail.store(tail, std::memory_order_release);
            }

            Y_VERIFY_DEBUG(tail->Offset <= index);

            // Construct new value and publish it by releasing the new count
            void* ptr = tail->Values() + (index - tail->Offset);
            T* value = new (ptr) T(std::forward<TArgs>(args)...);
            Count.store(index + 1, std::memory_order_release);
            return *value;
        }

        /**
         * Returns a thread-safe size of the container
         */
        size_t size() const {
            return Count.load(std::memory_order_acquire);
        }

        /**
         * Returns non thread-safe mutable reference, complexity is O(logN)
         */
        T& operator[](size_t index) {
            Y_VERIFY_DEBUG(index < Count.load(std::memory_order_relaxed));

            return *FindPtr(Tail.load(std::memory_order_relaxed), index);
        }

        /**
         * Returns a thread-safe immutable reference, complexity is O(logN)
         */
        const T& operator[](size_t index) const {
            Y_VERIFY_DEBUG(index < size());

            return *FindPtr(Tail.load(std::memory_order_acquire), index);
        }

    private:
        static T* FindPtr(TChunk* tail, size_t index) {
            while (tail && index < tail->Offset) {
                tail = tail->Prev;
            }

            Y_VERIFY_DEBUG(tail && index < tail->EndOffset());

            return tail->Values() + (index - tail->Offset);
        }

    private:
        static TChunk* AllocateChunk(size_t offset, size_t bytes, TChunk* prev) {
            void* base = ::operator new(bytes);
            return new (base) TChunk(offset, bytes, prev);
        }

        static void FreeChunk(TChunk* chunk) {
            void* base = chunk;
            ::operator delete(base);
        }

    private:
        std::atomic<size_t> Count{ 0 };
        std::atomic<TChunk*> Head{ nullptr };
        std::atomic<TChunk*> Tail{ nullptr };
    };

}
}
