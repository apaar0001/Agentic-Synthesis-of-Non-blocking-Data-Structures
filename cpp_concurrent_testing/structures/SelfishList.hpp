#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

/*
 * Selfish List - Lock-free linked list with wait-free contains
 * 
 * Based on "Why Non-Blocking Operations Should Be Selfish"
 * by Joel Gibson and Vincent Gramoli (DISC'15)
 * 
 * Key features:
 * - Wait-free contains operation
 * - Lock-free insert and remove with helping mechanism
 * - Uses marked pointers with flag and mark bits
 * - Backlinks for efficient helping
 */

class SelfishList : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;  // Encodes (right, mark, flag) as pointer with 2 LSBs
        Node* backlink;           // For helping mechanism
        
        Node(int v, Node* n = nullptr) : val(v), next(n), backlink(nullptr) {}
    };
    
    Node* head;
    
    // Marked pointer helpers
    // Bit 0: flag bit
    // Bit 1: mark bit
    // Remaining bits: actual pointer
    
    static bool is_marked(Node* n) {
        return (reinterpret_cast<uintptr_t>(n) & 2UL) != 0;
    }
    
    static bool is_flagged(Node* n) {
        return (reinterpret_cast<uintptr_t>(n) & 1UL) != 0;
    }
    
    static Node* get_right(Node* n) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(n) & ~3UL);
    }
    
    static Node* pack_tuple(Node* right, bool mark, bool flag) {
        uintptr_t tuple = reinterpret_cast<uintptr_t>(get_right(right));
        if (flag) tuple |= 1UL;
        if (mark) tuple |= 2UL;
        return reinterpret_cast<Node*>(tuple);
    }
    
    // Search forward from curr_node to find two nodes n1 and n2
    // satisfying n1.val <= val < n2.val
    void searchfrom(int val, Node* curr_node, Node** n1, Node** n2) {
        Node* next_node = get_right(curr_node->next.load(std::memory_order_acquire));
        
        while (next_node->val <= val) {
            // Ensure either next_node is unmarked, or both curr_node and next_node
            // are marked and curr_node was marked earlier
            Node* curr_next = curr_node->next.load(std::memory_order_acquire);
            Node* next_next = next_node->next.load(std::memory_order_acquire);
            
            while (is_marked(next_next) &&
                   (!is_marked(curr_next) || get_right(curr_next) != next_node)) {
                if (get_right(curr_next) == next_node) {
                    helpmarked(curr_node, next_node);
                }
                curr_next = curr_node->next.load(std::memory_order_acquire);
                next_node = get_right(curr_next);
                next_next = next_node->next.load(std::memory_order_acquire);
            }
            
            if (next_node->val <= val) {
                curr_node = next_node;
                next_node = get_right(curr_node->next.load(std::memory_order_acquire));
            }
        }
        
        *n1 = curr_node;
        *n2 = next_node;
    }
    
    // Search forward from curr_node to find two nodes n1 and n2
    // satisfying n1.val < val <= n2.val
    void searchfrom2(int val, Node* curr_node, Node** n1, Node** n2) {
        Node* next_node = get_right(curr_node->next.load(std::memory_order_acquire));
        
        while (next_node->val < val) {
            Node* curr_next = curr_node->next.load(std::memory_order_acquire);
            Node* next_next = next_node->next.load(std::memory_order_acquire);
            
            while (is_marked(next_next) &&
                   (!is_marked(curr_next) || get_right(curr_next) != next_node)) {
                if (get_right(curr_next) == next_node) {
                    helpmarked(curr_node, next_node);
                }
                curr_next = curr_node->next.load(std::memory_order_acquire);
                next_node = get_right(curr_next);
                next_next = next_node->next.load(std::memory_order_acquire);
            }
            
            if (next_node->val < val) {
                curr_node = next_node;
                next_node = get_right(curr_node->next.load(std::memory_order_acquire));
            }
        }
        
        *n1 = curr_node;
        *n2 = next_node;
    }
    
    // Assumes prev_node is flagged, del_node is marked, and prev_node.right = del_node
    // Attempts to swing prev_node.right to del_node.right
    void helpmarked(Node* prev_node, Node* del_node) {
        Node* next_node = get_right(del_node->next.load(std::memory_order_acquire));
        Node* expected = pack_tuple(del_node, false, true);
        prev_node->next.compare_exchange_strong(
            expected,
            pack_tuple(next_node, false, false),
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }
    
    // Assumes prev_node is flagged and prev_node.right = del_node
    // Attempts to mark and then physically remove del_node
    void helpflagged(Node* prev_node, Node* del_node) {
        del_node->backlink = prev_node;
        if (!is_marked(del_node->next.load(std::memory_order_acquire))) {
            trymark(del_node);
        }
        helpmarked(prev_node, del_node);
    }
    
    // Assumes del_node is preceded by a flagged node
    // Attempts to mark the node del_node
    void trymark(Node* del_node) {
        do {
            Node* next_node = get_right(del_node->next.load(std::memory_order_acquire));
            Node* expected = pack_tuple(next_node, false, false);
            del_node->next.compare_exchange_strong(
                expected,
                pack_tuple(next_node, true, false),
                std::memory_order_acq_rel,
                std::memory_order_acquire);
            
            if (is_flagged(expected)) {
                // Failure due to del_node becoming flagged
                helpflagged(del_node, get_right(expected));
            }
        } while (!is_marked(del_node->next.load(std::memory_order_acquire)));
    }
    
    // Attempts to flag the predecessor of target_node
    // Returns true if this call performed the flagging
    // Sets ret_node to the flagged predecessor, or nullptr if target was deleted
    bool tryflag(Node* prev_node, Node* target_node, Node** ret_node) {
        for (;;) {
            Node* prev_next = prev_node->next.load(std::memory_order_acquire);
            
            if (prev_next == pack_tuple(target_node, false, true)) {
                // Predecessor already flagged
                *ret_node = prev_node;
                return false;
            }
            
            // Attempt to flag
            Node* expected = pack_tuple(target_node, false, false);
            bool success = prev_node->next.compare_exchange_strong(
                expected,
                pack_tuple(target_node, false, true),
                std::memory_order_acq_rel,
                std::memory_order_acquire);
            
            if (expected == pack_tuple(target_node, false, false)) {
                // Successful flagging
                *ret_node = prev_node;
                return success;
            }
            if (expected == pack_tuple(target_node, false, true)) {
                // Some concurrent op flagged it first
                *ret_node = prev_node;
                return false;
            }
            
            // Possibly a fail due to marking. Follow backlinks to something unmarked
            while (is_marked(prev_node->next.load(std::memory_order_acquire))) {
                prev_node = prev_node->backlink;
            }
            
            Node* del_node;
            searchfrom2(target_node->val, prev_node, &prev_node, &del_node);
            if (del_node != target_node) {
                // Target got deleted
                *ret_node = nullptr;
                return false;
            }
        }
    }

public:
    SelfishList() {
        // Create sentinel nodes
        Node* tail = new Node(INT_MAX, nullptr);
        head = new Node(INT_MIN, tail);
    }
    
    ~SelfishList() override {
        Node* curr = head;
        while (curr) {
            Node* next = get_right(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }
    
    // Wait-free contains operation
    bool contains(int key) override {
        Node* curr = head;
        bool marked = false;
        
        while (curr->val < key) {
            Node* curr_next = curr->next.load(std::memory_order_acquire);
            marked = is_marked(curr_next);
            curr = get_right(curr_next);
        }
        
        return (curr->val == key && !marked);
    }
    
    // Lock-free insert with helping
    bool add(int key) override {
        Node* prev_node;
        Node* next_node;
        searchfrom(key, head, &prev_node, &next_node);
        
        if (prev_node->val == key) {
            return false;  // Already exists
        }
        
        Node* newnode = new Node(key, nullptr);
        
        for (;;) {
            Node* prev_next = prev_node->next.load(std::memory_order_acquire);
            
            if (is_flagged(prev_next)) {
                helpflagged(prev_node, get_right(prev_next));
            } else {
                newnode->next.store(pack_tuple(next_node, false, false), std::memory_order_relaxed);
                Node* expected = pack_tuple(next_node, false, false);
                bool success = prev_node->next.compare_exchange_strong(
                    expected,
                    pack_tuple(newnode, false, false),
                    std::memory_order_acq_rel,
                    std::memory_order_acquire);
                
                if (expected == pack_tuple(next_node, false, false)) {
                    // Success
                    return true;
                } else {
                    // Failure due to flagging?
                    if (is_flagged(expected)) {
                        helpflagged(prev_node, get_right(expected));
                    }
                    // May have to go through backlinks due to marking
                    while (is_marked(prev_node->next.load(std::memory_order_acquire))) {
                        prev_node = prev_node->backlink;
                    }
                }
            }
            
            searchfrom(key, prev_node, &prev_node, &next_node);
            if (prev_node->val == key) {
                delete newnode;
                return false;
            }
        }
    }
    
    // Lock-free remove with helping
    bool remove(int key) override {
        Node* prev_node;
        Node* del_node;
        searchfrom2(key, head, &prev_node, &del_node);
        
        if (del_node->val != key) {
            return false;  // Not found
        }
        
        bool result = tryflag(prev_node, del_node, &prev_node);
        if (prev_node != nullptr) {
            helpflagged(prev_node, del_node);
        }
        
        return result;
    }
};
