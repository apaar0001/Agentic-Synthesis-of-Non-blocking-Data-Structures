package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Linked List with Sentinel Nodes.
 *
 * Converted from lock-based (hand-over-hand / lock-coupling with ReentrantLock)
 * to fully lock-free using AtomicMarkableReference.
 *
 * Distinguishing characteristics preserved from the original variant:
 * - NodePair / search() naming (instead of Window / find())
 * - Inner node class named "ListNode" with field "value" (not "key")
 * - Explicit sentinel nodes (head=MIN_VALUE, tail=MAX_VALUE)
 *
 * Lock-freedom: all operations loop on CAS; no thread holds any lock.
 */
public class LinkedListSentinelNode implements SetADT {

    private static class ListNode {
        final int value;
        final AtomicMarkableReference<ListNode> next;

        ListNode(int value) {
            this.value = value;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    // Sentinel nodes bound all valid keys
    private final ListNode head;
    private final ListNode tail;

    public LinkedListSentinelNode() {
        head = new ListNode(Integer.MIN_VALUE);
        tail = new ListNode(Integer.MAX_VALUE);
        head.next.set(tail, false);
    }

    private static class NodePair {
        final ListNode left, right;

        NodePair(ListNode left, ListNode right) {
            this.left = left;
            this.right = right;
        }
    }

    /**
     * CAS-based traversal: returns (left, right) s.t. right.value >= val.
     * Unlinks marked nodes eagerly during traversal.
     */
    private NodePair search(int val) {
        ListNode left, right, succ;
        boolean[] isMarked = { false };
        retry: while (true) {
            left = head;
            right = left.next.getReference();
            while (true) {
                succ = right.next.get(isMarked);
                while (isMarked[0]) {
                    if (!left.next.compareAndSet(right, succ, false, false)) {
                        continue retry;
                    }
                    right = succ;
                    succ = right.next.get(isMarked);
                }
                if (right.value >= val)
                    return new NodePair(left, right);
                left = right;
                right = succ;
            }
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            NodePair pair = search(key);
            if (pair.right.value == key)
                return false;
            ListNode newNode = new ListNode(key);
            newNode.next.set(pair.right, false);
            if (pair.left.next.compareAndSet(pair.right, newNode, false, false))
                return true;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            NodePair pair = search(key);
            if (pair.right.value != key)
                return false;
            ListNode succ = pair.right.next.getReference();
            // Logical deletion: mark pair.right.next
            if (!pair.right.next.compareAndSet(succ, succ, false, true))
                continue;
            // Node has been marked
            // Best-effort physical removal; search() cleans up on next call
            pair.left.next.compareAndSet(pair.right, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] isMarked = { false };
        ListNode curr = head;
        while (curr.value < key) {
            curr = curr.next.getReference();
        }
        curr.next.get(isMarked);
        return (curr.value == key && !isMarked[0]);
    }
}
