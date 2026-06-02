package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Linked List using Optimistic CAS Synchronization.
 *
 * Variant characteristics:
 * - Uses AtomicMarkableReference like Harris-Michael, but uses a different
 * traversal strategy with no explicit "retry" label restart in find().
 * - remove() atomically marks and physically unlinks in a single CAS attempt.
 * - contains() is completely wait-free: it traverses without retrying.
 *
 * Lock-freedom: add(), remove() loop on CAS with guaranteed progress;
 * contains() is wait-free (single linear traversal, no CAS).
 */
public class LinkedListOptimisticCAS implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;
    private final Node tail;

    public LinkedListOptimisticCAS() {
        head = new Node(Integer.MIN_VALUE);
        tail = new Node(Integer.MAX_VALUE);
        head.next.set(tail, false);
    }

    private static class Window {
        final Node pred, curr;

        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }

    private Window find(int key) {
        Node pred, curr, succ;
        boolean[] marked = { false };
        retry: while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    // Eagerly unlink marked nodes
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        continue retry;
                    }
                    curr = succ;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key)
                    return new Window(pred, curr);
                pred = curr;
                curr = succ;
            }
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Window w = find(key);
            if (w.curr.key == key)
                return false;
            Node newNode = new Node(key);
            newNode.next.set(w.curr, false);
            if (w.pred.next.compareAndSet(w.curr, newNode, false, false))
                return true;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Window w = find(key);
            if (w.curr.key != key)
                return false;
            Node succ = w.curr.next.getReference();
            // Mark curr as logically deleted
            if (!w.curr.next.compareAndSet(succ, succ, false, true))
                continue;
            // Node has been marked
            // Attempt immediate physical removal (optimistic: may fail, find() cleans up)
            w.pred.next.compareAndSet(w.curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        // Wait-free traversal: no CAS, no retry needed
        boolean[] marked = { false };
        Node curr = head.next.getReference();
        while (curr.key < key) {
            curr = curr.next.getReference();
        }
        curr.next.get(marked);
        return (curr.key == key && !marked[0]);
    }
}
