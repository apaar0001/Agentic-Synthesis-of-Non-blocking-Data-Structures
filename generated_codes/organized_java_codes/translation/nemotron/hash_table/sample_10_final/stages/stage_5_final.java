package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static final int DEFAULT_CAPACITY = 16;
    private static final float LOAD_FACTOR = 0.75f;

    private final AtomicReference<Node[]> table;
    private final AtomicInteger size;
    private final int threshold;

    public ConcurrentDataStructure() {
        int cap = DEFAULT_CAPACITY;
        @SuppressWarnings("unchecked")
        AtomicReference<Node[]> tmp = new AtomicReference<>(new AtomicReference[cap]);
        table = tmp;
        for (int i = 0; i < cap; i++) {
            table.get()[i] = new AtomicReference<>(null);
        }
        size = new AtomicInteger(0);
        threshold = (int) (cap * LOAD_FACTOR);
    }

    @Override
    public boolean add(int key) {
        int h = hash(key);
        while (true) {
            Node[] tab = table.get();
            int cap = tab.length;
            int idx = h & (cap - 1);
            AtomicReference<Node> bucket = tab[idx];

            Node pred = null;
            Node curr = bucket.getReference();
            boolean found = false;

            while (curr != null) {
                if (curr.next.isMarked()) {
                    Node next = curr.next.getReference();
                    if (pred == null) {
                        bucket.compareAndSet(curr, next);
                    } else {
                        pred.next.compareAndSet(curr, next, false, false);
                    }
                    curr = (pred == null) ? bucket.getReference() : pred.next.getReference();
                    continue;
                }
                if (curr.key == key) {
                    found = true;
                    break;
                }
                pred = curr;
                curr = curr.next.getReference();
            }

            if (found) {
                return false;
            }

            Node newNode = new Node(key, null);
            if (pred == null) {
                if (bucket.compareAndSet(null, newNode)) {
                    break;
                }
            } else {
                if (pred.next.compareAndSet(curr, newNode, false, false)) {
                    break;
                }
            }
        }

        int newSize = size.incrementAndGet();
        if (newSize > threshold) {
            resize();
        }
        return true;
    }

    @Override
    public boolean remove(int key) {
        int h = hash(key);
        while (true) {
            Node[] tab = table.get();
            int cap = tab.length;
            int idx = h & (cap - 1);
            AtomicReference<Node> bucket = tab[idx];

            Node pred = null;
            Node curr = bucket.getReference();
            while (curr != null) {
                if (curr.next.isMarked()) {
                    Node next = curr.next.getReference();
                    if (pred == null) {
                        bucket.compareAndSet(curr, next);
                    } else {
                        pred.next.compareAndSet(curr, next, false, false);
                    }
                    curr = (pred == null) ? bucket.getReference() : pred.next.getReference();
                    continue;
                }
                if (curr.key == key) {
                    if (curr.next.compareAndSet(curr.next.getReference(), curr.next.getReference(), false, true)) {
                        // Node has been marked
                        Node next = curr.next.getReference();
                        if (pred == null) {
                            bucket.compareAndSet(curr, next);
                        } else {
                            pred.next.compareAndSet(curr, next, false, false);
                        }
                        size.decrementAndGet();
                        return true;
                    } else {
                        break;
                    }
                }
                pred = curr;
                curr = curr.next.getReference();
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int h = hash(key);
        Node[] tab = table.get();
        int cap = tab.length;
        int idx = h & (cap - 1);
        AtomicReference<Node> bucket = tab[idx];

        Node curr = bucket.getReference();
        while (curr != null) {
            if (curr.next.isMarked()) {
                curr = curr.next.getReference();
                continue;
            }
            if (curr.key == key) {
                return true;
            }
            curr = curr.next.getReference();
        }
        return false;
    }

    private void resize() {
        Node[] oldTab = table.get();
        int oldCap = oldTab.length;
        int newCap = oldCap << 1;
        @SuppressWarnings("unchecked")
        AtomicReference<Node[]> newTabRef = new AtomicReference<>(new AtomicReference[newCap]);
        Node[] newTab = newTabRef.get();
        for (int i = 0; i < newCap; i++) {
            newTab[i] = new AtomicReference<>(null);
        }

        for (int i = 0; i < oldCap; i++) {
            Node curr = oldTab[i].getReference();
            while (curr != null) {
                int idx = hash(curr.key) & (newCap - 1);
                Node next = curr.next.getReference();
                AtomicReference<Node> bucket = newTab[idx];
                while (true) {
                    Node oldHead = bucket.getReference();
                    curr.next.set(oldHead, false);
                    if (bucket.compareAndSet(oldHead, curr)) {
                        break;
                    }
                }
                curr = next;
            }
        }

        table.set(newTabRef);
        threshold = (int) (newCap * LOAD_FACTOR);
    }

    private static final class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private static int hash(int x) {
        x = Integer.hashCode(x);
        x ^= (x >>> 16);
        return x;
    }
}