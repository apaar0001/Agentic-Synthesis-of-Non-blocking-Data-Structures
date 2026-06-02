package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {

    private static final int INITIAL_CAPACITY = 16;
    private static final double LOAD_FACTOR = 0.75;

    private static class Node {
        final int key;
        final int hash;
        final AtomicMarkableReference<Node> next;

        Node(int key, int hash, Node next) {
            this.key = key;
            this.hash = hash;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private static class Window {
        final Node pred;
        final Node curr;
        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }

    private volatile AtomicReferenceArray<Node> buckets;
    private final AtomicInteger size;
    private final AtomicInteger capacity;

    public ConcurrentDataStructure() {
        int cap = INITIAL_CAPACITY;
        this.buckets = new AtomicReferenceArray<>(cap);
        this.size = new AtomicInteger(0);
        this.capacity = new AtomicInteger(cap);
        for (int i = 0; i < cap; i++) {
            buckets.set(i, new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, null));
        }
    }

    private int spread(int key) {
        int h = key ^ (key >>> 16);
        return h & 0x7fffffff;
    }

    private int getBucketIndex(int hash, int cap) {
        return hash % cap;
    }

    private Window find(Node head, int hash, int key) {
        Node pred, curr, succ;
        boolean[] marked = {false};
        retry:
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (curr != null) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    boolean snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip) continue retry;
                    curr = succ;
                    if (curr == null) break;
                    succ = curr.next.get(marked);
                }
                if (curr == null) break;
                if (curr.hash > hash || (curr.hash == hash && curr.key >= key)) {
                    return new Window(pred, curr);
                }
                pred = curr;
                curr = succ;
            }
            return new Window(pred, curr);
        }
    }

    @Override
    public boolean add(int key) {
        int hash = spread(key);
        while (true) {
            int cap = capacity.get();
            AtomicReferenceArray<Node> table = buckets;
            int idx = getBucketIndex(hash, cap);
            Node head = table.get(idx);
            if (head == null) {
                head = new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, null);
                if (!table.compareAndSet(idx, null, head)) continue;
            }
            Window w = find(head, hash, key);
            Node pred = w.pred;
            Node curr = w.curr;
            if (curr != null && curr.hash == hash && curr.key == key) {
                return false;
            }
            Node node = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                int newSize = size.incrementAndGet();
                if (newSize > (int)(cap * LOAD_FACTOR)) {
                    tryResize(cap);
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = spread(key);
        while (true) {
            int cap = capacity.get();
            AtomicReferenceArray<Node> table = buckets;
            int idx = getBucketIndex(hash, cap);
            Node head = table.get(idx);
            if (head == null) return false;
            Window w = find(head, hash, key);
            Node pred = w.pred;
            Node curr = w.curr;
            if (curr == null || curr.hash != hash || curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            boolean marked = curr.next.attemptMark(succ, true);
            if (!marked) continue;
            // Node has been marked
            size.decrementAndGet();
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = spread(key);
        int cap = capacity.get();
        AtomicReferenceArray<Node> table = buckets;
        int idx = getBucketIndex(hash, cap);
        Node head = table.get(idx);
        if (head == null) return false;
        boolean[] marked = {false};
        Node curr = head.next.getReference();
        while (curr != null) {
            curr.next.get(marked);
            if (!marked[0] && curr.hash == hash && curr.key == key) {
                return true;
            }
            if (curr.hash > hash || (curr.hash == hash && curr.key > key)) {
                return false;
            }
            curr = curr.next.getReference();
        }
        return false;
    }

    private void tryResize(int oldCap) {
        int newCap = oldCap * 2;
        if (!capacity.compareAndSet(oldCap, newCap)) return;
        AtomicReferenceArray<Node> newTable = new AtomicReferenceArray<>(newCap);
        for (int i = 0; i < newCap; i++) {
            newTable.set(i, new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, null));
        }
        AtomicReferenceArray<Node> oldTable = buckets;
        for (int i = 0; i < oldCap; i++) {
            Node head = oldTable.get(i);
            if (head == null) continue;
            boolean[] marked = {false};
            Node curr = head.next.getReference();
            while (curr != null) {
                Node succ = curr.next.get(marked);
                if (!marked[0]) {
                    int newIdx = getBucketIndex(curr.hash, newCap);
                    Node newHead = newTable.get(newIdx);
                    insertInto(newHead, curr.key, curr.hash);
                }
                curr = succ;
            }
        }
        buckets = newTable;
    }

    private void insertInto(Node head, int key, int hash) {
        while (true) {
            Window w = find(head, hash, key);
            Node pred = w.pred;
            Node curr = w.curr;
            if (curr != null && curr.hash == hash && curr.key == key) return;
            Node node = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, node, false, false)) return;
        }
    }
}