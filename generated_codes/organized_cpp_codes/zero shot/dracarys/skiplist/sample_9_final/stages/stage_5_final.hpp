#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
public:
    static const int MAX_LEVEL = 16;
    static const int MAX_HEIGHT = MAX_LEVEL - 1;

    struct Node {
        int val;
        std::atomic<Node*> forward[MAX_LEVEL];
        int topLevel;

        Node(int val, int height) : val(val), topLevel(height) {
            for (int i = 0; i < MAX_LEVEL; i++) {
                forward[i] = nullptr;
            }
        }
    };

    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_HEIGHT);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head->forward[i] = new Node(INT_MAX, MAX_HEIGHT);
        }
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* prev[MAX_LEVEL];
        Node* curr = head;
        for (int i = MAX_HEIGHT; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    prev[i] = curr;
                    curr = next;
                    break;
                }
                curr = next;
            }
        }
        return curr != nullptr && curr->val == key;
    }

    bool add(int key) override {
        Node* prev[MAX_LEVEL];
        Node* curr = head;
        for (int i = MAX_HEIGHT; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    prev[i] = curr;
                    curr = next;
                    break;
                }
                curr = next;
            }
        }
        if (curr != nullptr && curr->val == key) {
            return false;
        }
        int height = randomLevel();
        Node* newNode = new Node(key, height);
        for (int i = 0; i < height; i++) {
            newNode->forward[i] = prev[i]->forward[i].load(std::memory_order_relaxed);
        }
        for (int i = 0; i < height; i++) {
            while (!prev[i]->forward[i].compare_exchange_strong(newNode->forward[i], newNode, std::memory_order_acq_rel)) {
                newNode->forward[i] = prev[i]->forward[i].load(std::memory_order_relaxed);
            }
        }
        return true;
    }

    bool remove(int key) override {
        Node* prev[MAX_LEVEL];
        Node* curr = head;
        for (int i = MAX_HEIGHT; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    prev[i] = curr;
                    curr = next;
                    break;
                }
                curr = next;
            }
        }
        if (curr == nullptr || curr->val != key) {
            return false;
        }
        for (int i = 0; i < curr->topLevel; i++) {
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    prev[i]->forward[i].compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    break;
                }
                curr = next;
            }
        }
        for (int i = 0; i < curr->topLevel; i++) {
            curr->forward[i].store(get_marked_ref(curr->forward[i].load(std::memory_order_relaxed)), std::memory_order_relaxed);
        }
        delete curr;
        return true;
    }

private:
    static int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(0, 1);
        int height = 1;
        while (height < MAX_HEIGHT && dis(gen)) {
            height++;
        }
        return height;
    }

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* head;
};