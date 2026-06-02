package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Linked List with Lazy Deletion.
 *
 * Converted from lock-based (ReentrantLock) to fully lock-free using
 * AtomicMarkableReference. Preserves the "lazy deletion" philosophy:
 * - remove() performs ONLY logical deletion (mark the node)
 * - Physical unlinking is deferred to the next find() traversal
 *
 * This lazy approach reduces contention since remove() never needs to
 * CAS pred.next; cleanup is amortized across future operations.
 *
 * Lock-freedom: all operations loop on CAS; no thread can block another.
 */
public class LinkedListLazyDeletion implements SetADT {

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

    public LinkedListLazyDeletion() {
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

    /**
     * Traverses list finding position for key, lazily unlinking marked nodes.
     * This is where physical deletion actually occurs for lazy-marked nodes.
     */
    private Window find(int key) {
        Node pred, curr, succ;
        boolean[] marked = { false };
        retry: while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    // Lazy physical deletion: unlink the marked node now
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
            Node node = new Node(key);
            node.next.set(w.curr, false);
            if (w.pred.next.compareAndSet(w.curr, node, false, false))
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
            // Linearization point: logically mark the node (lazy variant)
            // Physical unlinking is intentionally deferred to find()
            if (w.curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                return true;
            }
            // CAS failed: another thread modified curr.next, retry find
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = { false };
        Node curr = head;
        while (curr.key < key) {
            curr = curr.next.getReference();
        }
        curr.next.get(marked);
        return (curr.key == key && !marked[0]);
    }
}
