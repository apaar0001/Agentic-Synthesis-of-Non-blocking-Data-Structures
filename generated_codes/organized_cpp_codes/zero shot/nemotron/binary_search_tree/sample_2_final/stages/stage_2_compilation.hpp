#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        explicit Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    Node* head;          // dummy root
    Node* minSentinel;   // holds INT_MIN
    Node* maxSentinel;   // holds INT_MAX

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    // returns node with key if found, otherwise nullptr.
    // sets parent and isLeftChild to the parent of the returned node (or insertion point) and whether it is a left child.
    Node* findNode(int key, Node** parent, bool* isLeftChild) {
        Node* curr = head->left.load(std::memory_order_acquire);
        Node* par = head;
        bool left = true; // direction from par to curr

        while (true) {
            Node* uncurr = get_unmarked_ref(curr);
            if (!uncurr) {
                *parent = par;
                *isLeftChild = left;
                return nullptr;
            }
            if (is_marked_ref(curr)) {
                // help remove the marked node
                helpRemove(par, uncurr, left);
                // restart search from root to maintain consistency
                curr = head->left.load(std::memory_order_acquire);
                par = head;
                left = true;
                continue;
            }
            if (uncurr->val == key) {
                *parent = par;
                *isLeftChild = left;
                return uncurr;
            }
            if (key < uncurr->val) {
                par = uncurr;
                curr = uncurr->left.load(std::memory_order_acquire);
                left = true;
            } else {
                par = uncurr;
                curr = uncurr->right.load(std::memory_order_acquire);
                left = false;
            }
        }
    }

    // attempts to physically remove a marked node from its parent
    void helpRemove(Node* parent, Node* node, bool isLeftChild) {
        std::atomic<Node*>& childPtr = isLeftChild ? parent->left : parent->right;
        Node* expected = get_marked_ref(node);
        Node* replacement = nullptr;

        // choose replacement: prefer non-null child
        Node* leftUnmarked = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* rightUnmarked = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        if (leftUnmarked) replacement = leftUnmarked;
        else if (rightUnmarked) replacement = rightUnmarked;

        Node* desired = get_unmarked_ref(replacement);
        childPtr.compare_exchange_strong(expected, desired,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire);
    }

    // finds the inorder successor (leftmost node) of given node
    Node* getSuccessor(Node* node) {
        Node* succ = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        while (succ) {
            Node* left = get_unmarked_ref(succ->left.load(std::memory_order_acquire));
            if (!left) break;
            succ = left;
        }
        return succ;
    }

public:
    ConcurrentDataStructure() {
        minSentinel = new Node(INT_MIN);
        maxSentinel = new Node(INT_MAX);
        head = new Node(0); // dummy value, not used for ordering
        head->left.store(minSentinel, std::memory_order_release);
        head->right.store(maxSentinel, std::memory_order_release);
        minSentinel->left.store(nullptr, std::memory_order_relaxed);
        minSentinel->right.store(nullptr, std::memory_order_relaxed);
        maxSentinel->left.store(nullptr, std::memory_order_relaxed);
        maxSentinel->right.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        // simple recursive deletion using unmarked pointers; assumes no concurrent ops during destruction
        std::function<void(Node*)> destroy = [&](Node* n) {
            if (!n) return;
            Node* left = get_unmarked_ref(n->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(n->right.load(std::memory_order_acquire));
            destroy(left);
            destroy(right);
            delete n;
        };
        Node* root = get_unmarked_ref(head->left.load(std::memory_order_acquire));
        destroy(root);
        delete minSentinel;
        delete maxSentinel;
        delete head;
    }

    bool contains(int key) override {
        Node* curr = head->left.load(std::memory_order_acquire);
        while (curr) {
            Node* uncurr = get_unmarked_ref(curr);
            if (!uncurr) break;
            if (is_marked_ref(curr)) {
                // skip marked node
                curr = head->left.load(std::memory_order_acquire);
                continue;
            }
            if (uncurr->val == key) return true;
            if (key < uncurr->val) {
                curr = uncurr->left.load(std::memory_order_acquire);
            } else {
                curr = uncurr->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            bool isLeft = false;
            Node* found = findNode(key, &parent, &isLeft);
            if (found) return false; // duplicate

            Node* newNode = new Node(key);
            std::atomic<Node*>& childPtr = isLeft ? parent->left : parent->right;
            Node* expected = childPtr.load(std::memory_order_acquire);
            // expected should be nullptr (or a node that we will replace)
            if (childPtr.compare_exchange_strong(expected, newNode,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                return true;
            }
            // CAS failed, retry
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            bool isLeft = false;
            Node* toRemove = findNode(key, &parent, &isLeft);
            if (!toRemove) return false; // not present

            std::atomic<Node*>& childPtr = isLeft ? parent->left : parent->right;
            Node* curr = childPtr.load(std::memory_order_acquire);
            // help remove if already marked
            if (is_marked_ref(curr)) {
                helpRemove(parent, get_unmarked_ref(curr), isLeft);
                continue;
            }
            // logical deletion: mark the node
            Node* marked = get_marked_ref(curr);
            if (!childPtr.compare_exchange_strong(curr, marked,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                continue; // retry
            }

            // Now physically remove the marked node
            Node* leftUnmarked = get_unmarked_ref(toRemove->left.load(std::memory_order_acquire));
            Node* rightUnmarked = get_unmarked_ref(toRemove->right.load(std::memory_order_acquire));

            Node* replacement = nullptr;
            if (!leftUnmarked && !rightUnmarked) {
                replacement = nullptr;
            } else if (!leftUnmarked) {
                replacement = rightUnmarked;
            } else if (!rightUnmarked) {
                replacement = leftUnmarked;
            } else {
                // two children: replace with successor
                Node* succ = getSuccessor(toRemove);
                // copy successor's value
                toRemove->val = succ->val;
                // now remove successor (which has at most one child)
                toRemove = succ;
                // recompute parent and direction for successor
                // find parent of successor (could be toRemove or deeper)
                Node* par = head;
                bool leftDir = true;
                Node* cur = head->left.load(std::memory_order_acquire);
                while (cur) {
                    Node* ucur = get_unmarked_ref(cur);
                    if (!ucur) break;
                    if (ucur == toRemove) break;
                    if (toRemove->val < ucur->val) {
                        par = ucur;
                        cur = ucur->left.load(std::memory_order_acquire);
                        leftDir = true;
                    } else {
                        par = ucur;
                        cur = ucur->right.load(std::memory_order_acquire);
                        leftDir = false;
                    }
                }
                parent = par;
                isLeft = leftDir;
                childPtr = isLeft ? parent->left : parent->right;
                curr = childPtr.load(std::memory_order_acquire);
                // mark successor
                marked = get_marked_ref(curr);
                if (!childPtr.compare_exchange_strong(curr, marked,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    continue; // retry from start
                }
                // after marking successor, compute its children for replacement
                leftUnmarked = get_unmarked_ref(toRemove->left.load(std::memory_order_acquire));
                rightUnmarked = get_unmarked_ref(toRemove->right.load(std::memory_order_acquire));
                if (leftUnmarked) replacement = leftUnmarked;
                else if (rightUnmarked) replacement = rightUnmarked;
                else replacement = nullptr;
            }

            Node* desired = get_unmarked_ref(replacement);
            if (childPtr.compare_exchange_strong(curr, desired,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                delete toRemove;
                return true;
            }
            // CAS failed, retry
        }
    }
};