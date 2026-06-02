package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    private final int capacity;
    private final AtomicMarkableReference<Node>[] table;

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.table = new AtomicMarkableReference[capacity];
        for (int i = 0; i < capacity; i++) {
            table[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    private int index(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int idx = index(key);
        while (true) {
            Node curr = table[idx].getReference();
            Node prev = null;

            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                if (marked[0]) {
                    if (prev == null) {
                        if (!table[idx].compareAndSet(curr, next, false, false)) {
                            break;
                        }
                    } else {
                        if (!prev.next.compareAndSet(curr, next, false, false)) {
                            break;
                        }
                    }
                    curr = table[idx].getReference();
                    prev = null;
                    continue;
                }
                if (curr.key == key) {
                    return false;
                }
                prev = curr;
                curr = next;
            }

            Node newNode = new Node(key, table[idx].getReference());
            if (prev == null) {
                if (table[idx].compareAndSet(table[idx].getReference(), newNode, false, false)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = index(key);
        while (true) {
            Node curr = table[idx].getReference();
            Node prev = null;

            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                if (marked[0]) {
                    if (prev == null) {
                        if (!table[idx].compareAndSet(curr, next, false, false)) {
                            break;
                        }
                    } else {
                        if (!prev.next.compareAndSet(curr, next, false, false)) {
                            break;
                        }
                    }
                    curr = table[idx].getReference();
                    prev = null;
                    continue;
                }
                if (curr.key == key) {
                    if (curr.next.attemptMark(next, true)) {
                        // Node has been marked
                        if (prev == null) {
                            table[idx].compareAndSet(curr, next, false, false);
                        } else {
                            prev.next.compareAndSet(curr, next, false, false);
                        }
                        return true;
                    } else {
                        break;
                    }
                }
                prev = curr;
                curr = next;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = index(key);
        Node curr = table[idx].getReference();
        while (curr != null) {
            boolean[] marked = {false};
            Node next = curr.next.get(marked);
            if (!marked[0] && curr.key == key) {
                return true;
            }
            curr = next;
        }
        return false;
    }
}