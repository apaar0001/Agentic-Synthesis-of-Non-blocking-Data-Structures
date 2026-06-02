#pragma once

#include "../utils/SetADT.hpp"
#include "../utils/QueueADT.hpp"
#include "../utils/StackADT.hpp"

// ── Set / List structures ────────────────────────────────────────────────────
#include "../structures/LockFreeList.hpp"
#include "../structures/LazyList.hpp"
#include "../structures/LockCouplingList.hpp"
#include "../structures/LockBasedHashTable.hpp"
#include "../structures/SequentialSkipList.hpp"
#include "../structures/TreeLock.hpp"
#include "../structures/SelfishList.hpp"
#include "../structures/VersionedList.hpp"
#include "../structures/LockFreeHashTable.hpp"
#include "../structures/SequentialHashTable.hpp"
#include "../structures/AVLTree.hpp"
#include "../structures/NewAVLTree.hpp"
#include "../structures/RBTree.hpp"
#include "../structures/SkipListLock.hpp"
#include "../structures/FraserSkipList.hpp"
#include "../structures/NoHotspotSkipList.hpp"
#include "../structures/RotatingSkipList.hpp"
#include "../structures/NumaskSkipList.hpp"
#include "../structures/SFTree.hpp"
#include "../structures/LockFreeBSTree.hpp"

// ── Stack structures ─────────────────────────────────────────────────────────
#include "../structures/StackHarrisMichael.hpp"
#include "../structures/StackDifferentNames.hpp"
#include "../structures/StackLazyDeletion.hpp"
#include "../structures/StackOptimisticCAS.hpp"
#include "../structures/StackSentinelNode.hpp"

// ── Queue structures ─────────────────────────────────────────────────────────
#include "../structures/QueueHarrisMichael.hpp"
#include "../structures/QueueDifferentNames.hpp"
#include "../structures/QueueLazyDeletion.hpp"
#include "../structures/QueueOptimisticCAS.hpp"
#include "../structures/QueueSentinelNode.hpp"

#include <memory>
#include <string>

// LLM-generated code — included once; the pipeline overwrites this file per run.
// Only include when actually compiling for generated code testing.
#if defined(GENERATED_IS_SET) || defined(GENERATED_IS_STACK) || defined(GENERATED_IS_QUEUE)
#include "ConcurrentDataStructure.hpp"
#endif

inline std::unique_ptr<SetADT> createSet(const std::string& algo) {
    if (algo == "lockfree-list") {
        return std::make_unique<LockFreeList>();
    }
    if (algo == "lazy-list") {
        return std::make_unique<LazyList>();
    }
    if (algo == "lock-coupling-list") {
        return std::make_unique<LockCouplingList>();
    }
    if (algo == "lockbased-ht") {
        return std::make_unique<LockBasedHashTable>();
    }
    if (algo == "sequential-skiplist") {
        return std::make_unique<SequentialSkipList>();
    }
    if (algo == "tree-lock") {
        return std::make_unique<TreeLock>();
    }
    if (algo == "selfish-list") {
        return std::make_unique<SelfishList>();
    }
    if (algo == "versioned-list") {
        return std::make_unique<VersionedList>();
    }
    if (algo == "lockfree-ht") {
        return std::make_unique<LockFreeHashTable>();
    }
    if (algo == "sequential-ht") {
        return std::make_unique<SequentialHashTable>();
    }
    if (algo == "avl") {
        return std::make_unique<AVLTree>();
    }
    if (algo == "newavl") {
        return std::make_unique<NewAVLTree>();
    }
    if (algo == "rbtree") {
        return std::make_unique<RBTree>();
    }
    if (algo == "skiplist-lock") {
        return std::make_unique<SkipListLock>();
    }
    if (algo == "fraser") {
        return std::make_unique<FraserSkipList>();
    }
    if (algo == "nohotspot") {
        return std::make_unique<NoHotspotSkipList>();
    }
    if (algo == "rotating") {
        return std::make_unique<RotatingSkipList>();
    }
    if (algo == "numask") {
        return std::make_unique<NumaskSkipList>();
    }
    if (algo == "sftree") {
        return std::make_unique<SFTree>();
    }
    if (algo == "lfbstree") {
        return std::make_unique<LockFreeBSTree>();
    }
#ifdef GENERATED_IS_SET
    if (algo == "generated") {
        return std::make_unique<ConcurrentDataStructure>();
    }
#endif
    return nullptr;
}

inline std::unique_ptr<StackADT> createStack(const std::string& algo) {
    if (algo == "stack_harris_michael") {
        return std::make_unique<StackHarrisMichael>();
    }
    if (algo == "stack_different_names") {
        return std::make_unique<StackDifferentNames>();
    }
    if (algo == "stack_lazy_deletion") {
        return std::make_unique<StackLazyDeletion>();
    }
    if (algo == "stack_optimistic_cas") {
        return std::make_unique<StackOptimisticCAS>();
    }
    if (algo == "stack_sentinel_node") {
        return std::make_unique<StackSentinelNode>();
    }
#ifdef GENERATED_IS_STACK
    if (algo == "generated") {
        return std::make_unique<ConcurrentDataStructure>();
    }
#endif
    return nullptr;
}

inline std::unique_ptr<QueueADT> createQueue(const std::string& algo) {
    if (algo == "queue_harris_michael") {
        return std::make_unique<QueueHarrisMichael>();
    }
    if (algo == "queue_different_names") {
        return std::make_unique<QueueDifferentNames>();
    }
    if (algo == "queue_lazy_deletion") {
        return std::make_unique<QueueLazyDeletion>();
    }
    if (algo == "queue_optimistic_cas") {
        return std::make_unique<QueueOptimisticCAS>();
    }
    if (algo == "queue_sentinel_node") {
        return std::make_unique<QueueSentinelNode>();
    }
#ifdef GENERATED_IS_QUEUE
    if (algo == "generated") {
        return std::make_unique<ConcurrentDataStructure>();
    }
#endif
    return nullptr;
}
