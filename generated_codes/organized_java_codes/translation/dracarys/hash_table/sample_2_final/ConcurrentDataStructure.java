package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static final int INITIAL_CAPACITY = 16;
    private static final int MAX_CAPACITY = 1 << 30;
    private static final float LOAD_FACTOR = 0.75f;

    private final AtomicReference<Node>[] table;
    private final AtomicInteger size;

    public ConcurrentDataStructure() {
        table = new AtomicReference[INITIAL_CAPACITY];
        size = new AtomicInteger(0);
        for (int i = 0; i < table.length; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return Math.abs(key % table.length);
    }

    private Node newNode(int key) {
        return new Node(key);
    }

    @Override
    public boolean add(int key) {
        int index = hash(key);
        AtomicReference<Node> ref = table[index];
        Node node = ref.getReference();
        while (node != null) {
            if (node.key == key && !node.marked) {
                return false;
            }
            node = node.next.getReference();
        }
        Node newNode = newNode(key);
        while (true) {
            node = ref.getReference();
            newNode.next.set(node);
            if (ref.compareAndSet(node, newNode)) {
                if (size.incrementAndGet() > table.length * LOAD_FACTOR) {
                    resize();
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = hash(key);
        AtomicReference<Node> ref = table[index];
        while (true) {
            Node node = ref.getReference();
            Node prev = null;
            while (node != null) {
                if (node.key == key && !node.marked) {
                    if (node.next.compareAndSet(node.next.getReference(), node.next.getReference())) {
                        node.marked = true;
                        // Node has been marked
                        if (prev != null) {
                            prev.next.compareAndSet(node, node.next.getReference());
                        } else {
                            ref.compareAndSet(node, node.next.getReference());
                        }
                        size.decrementAndGet();
                        return true;
                    }
                } else if (node.marked) {
                    if (prev != null) {
                        prev.next.compareAndSet(node, node.next.getReference());
                    } else {
                        ref.compareAndSet(node, node.next.getReference());
                    }
                } else {
                    prev = node;
                }
                node = node.next.getReference();
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int index = hash(key);
        AtomicReference<Node> ref = table[index];
        Node node = ref.getReference();
        while (node != null) {
            if (node.key == key && !node.marked) {
                return true;
            }
            node = node.next.getReference();
        }
        return false;
    }

    private void resize() {
        if (table.length >= MAX_CAPACITY) {
            return;
        }
        AtomicReference<Node>[] newTable = new AtomicReference[table.length * 2];
        for (int i = 0; i < newTable.length; i++) {
            newTable[i] = new AtomicReference<>(null);
        }
        for (AtomicReference<Node> ref : table) {
            Node node = ref.getReference();
            while (node != null) {
                Node next = node.next.getReference();
                int newIndex = Math.abs(node.key % newTable.length);
                AtomicReference<Node> newRef = newTable[newIndex];
                while (true) {
                    Node newNext = newRef.getReference();
                    node.next.set(newNext);
                    if (newRef.compareAndSet(newNext, node)) {
                        break;
                    }
                }
                node = next;
            }
        }
        table = newTable;
    }

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;
        volatile boolean marked;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
}