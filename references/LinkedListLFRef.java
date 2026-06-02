package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free sorted linked list — Harris-Michael algorithm.
 *
 * Ground-truth reference for LockFreeBench annotation evaluation.
 *
 * Progress guarantee : LOCK_FREE
 * Algorithm : Harris-Michael (Harris 2001 + Michael 2002 cleanup)
 * ABA protection : mark bit in AtomicMarkableReference (logical deletion)
 * Linearization points:
 * add() — successful CAS on pred.next inserting new node [FIXED LP]
 * remove() — successful CAS marking curr.next (logical deletion) [FIXED LP]
 * contains() — read of curr when curr.key >= key (wait-free read) [FIXED LP]
 */
public class LinkedListLFRef implements SetADT {

    private static final class Node {
        final int key;
        // next.getReference() = successor node
        // next.isMarked() = true means this node is logically deleted
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private final Node head; // sentinel: Integer.MIN_VALUE
    private final Node tail; // sentinel: Integer.MAX_VALUE

    public LinkedListLFRef() {
        tail = new Node(Integer.MAX_VALUE, null);
        head = new Node(Integer.MIN_VALUE, tail);
    }

    // --------------- Window helper (find) -----------------------------------

    /** Window: predecessor and current node surrounding search key. */
    private static final class Window {
        final Node pred, curr;

        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }

    /**
     * Traverses the list to find the window around key.
     * Physically removes logically-deleted nodes (marked nodes) along the way.
     */
    private Window find(int key) {
        retry: while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                boolean[] marked = { false };
                Node succ = curr.next.get(marked);

                // Physically remove logically-deleted (marked) nodes
                while (marked[0]) {
                    boolean snip = pred.next.compareAndSet(curr, succ, false, false);
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

    // --------------- SetADT operations --------------------------------------

    @Override
    public boolean add(int key) {
        while (true) {
            Window w = find(key);
            Node pred = w.pred;
            Node curr = w.curr;
            if (curr.key == key)
                return false; // already present

            Node node = new Node(key, curr);
            // LINEARIZATION POINT: successful CAS inserts node before curr
            if (pred.next.compareAndSet(curr, node, false, false))
                return true;
            // CAS failed → another thread modified pred.next; retry
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Window w = find(key);
            Node curr = w.curr;
            if (curr.key != key)
                return false; // not present

            Node succ = curr.next.getReference();
            // LINEARIZATION POINT: logical deletion — mark curr.next
            boolean marked = curr.next.compareAndSet(succ, succ, false, true);
            if (!marked)
                continue; // retry if someone else marked first

            // Best-effort physical removal (find() will clean up if this fails)
            w.pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = { false };
        Node curr = head.next.getReference();
        while (curr.key < key) {
            curr = curr.next.get(marked);
        }
        // LINEARIZATION POINT: non-CAS read — wait-free
        return (curr.key == key) && !marked[0];
    }
}
