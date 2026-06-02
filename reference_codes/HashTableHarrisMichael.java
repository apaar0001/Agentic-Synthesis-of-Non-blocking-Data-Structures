package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Hash Table — Harris-Michael chaining (Ref 1).
 *
 * Structure: array of Bucket, each a sorted lock-free linked list.
 * Bucket uses Harris-Michael: AtomicMarkableReference for logical delete,
 * physical removal during find().
 * Names: Node/key/next, Bucket/Window/find(), capacity=1024
 *
 * Victim anchor is in the per-bucket remove() — the outer remove() delegates,
 * so the anchor fires inside the Bucket's remove() which is what the test
 * harness injects into via class-level search.
 */
public class HashTableHarrisMichael implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int k) {
            key = k;
            next = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Bucket {
        final Node head = new Node(Integer.MIN_VALUE);
        final Node tail = new Node(Integer.MAX_VALUE);

        Bucket() {
            head.next.set(tail, false);
        }

        private static class Window {
            Node pred, curr;

            Window(Node p, Node c) {
                pred = p;
                curr = c;
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
                        if (!pred.next.compareAndSet(curr, succ, false, false))
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

        boolean add(int key) {
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

        boolean remove(int key) {
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

        boolean contains(int key) {
            boolean[] marked = { false };
            Node curr = head;
            while (curr.key < key)
                curr = curr.next.getReference();
            curr.next.get(marked);
            return curr.key == key && !marked[0];
        }
    }

    private final Bucket[] table;
    private static final int CAPACITY = 1024;

    public HashTableHarrisMichael() {
        table = new Bucket[CAPACITY];
        for (int i = 0; i < CAPACITY; i++)
            table[i] = new Bucket();
    }

    private int hash(int key) {
        return Math.abs(key) % CAPACITY;
    }

    @Override
    public boolean add(int key) {
        return table[hash(key)].add(key);
    }

    @Override
    public boolean remove(int key) {
        return table[hash(key)].remove(key);
    }

    @Override
    public boolean contains(int key) {
        return table[hash(key)].contains(key);
    }
}
