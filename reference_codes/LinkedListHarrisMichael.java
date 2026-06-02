package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Harris-Michael concurrent lock-free linked list.
 *
 * Algorithm: Harris (2001) + Michael (2002)
 * - Logical deletion: marks node's next pointer via AtomicMarkableReference
 * - Physical deletion: CAS pred.next during find() traversal
 * - Linearization point for add(): successful CAS on pred.next
 * - Linearization point for remove(): successful mark CAS on curr.next
 *
 * Lock-freedom: every path through add/remove/contains makes CAS progress;
 * no thread can block another.
 */
public class LinkedListHarrisMichael implements SetADT {

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

    public LinkedListHarrisMichael() {
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
        boolean snip;
        retry: while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip)
                        continue retry;
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
            if (!w.curr.next.compareAndSet(succ, succ, false, true))
                continue;
            // Node has been marked
            w.pred.next.compareAndSet(w.curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = { false };
        Node curr = head;
        while (curr.key < key) {
            curr = curr.next.getReference();
            curr.next.get(marked);
        }
        return (curr.key == key && !marked[0]);
    }
}
