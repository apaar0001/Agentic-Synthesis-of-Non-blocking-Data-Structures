package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static final int INITIAL_CAPACITY = 16;
    private static final int MAX_CAPACITY = 1 << 30;

    private final AtomicReference<Node>[] table;

    public ConcurrentDataStructure() {
        table = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < table.length; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return key & (table.length - 1);
    }

    private boolean cas(Node[] nodes, int i, Node expected, Node value) {
        return nodes[i].getReference().compareAndSet(expected, value);
    }

    private Node newNode(int key, boolean marked) {
        return new Node(key, marked);
    }

    private Node[] resize() {
        int newCapacity = table.length << 1;
        if (newCapacity > MAX_CAPACITY) {
            return table;
        }
        AtomicReference<Node>[] newTable = new AtomicReference[newCapacity];
        for (int i = 0; i < newTable.length; i++) {
            newTable[i] = new AtomicReference<>(null);
        }
        for (int i = 0; i < table.length; i++) {
            Node node = table[i].getReference();
            while (node != null) {
                Node next = node.next.getReference();
                int index = node.key & (newCapacity - 1);
                Node newNext = newTable[index].getReference();
                node.next.set(new AtomicMarkableReference<>(newNext, false));
                newTable[index].set(node);
                node = next;
            }
        }
        return newTable;
    }

    @Override
    public boolean add(int key) {
        int index = hash(key);
        Node node = table[index].getReference();
        while (node != null) {
            if (node.key == key) {
                return false;
            }
            node = node.next.getReference();
        }
        Node newNode = newNode(key, false);
        Node prev = null;
        Node curr = table[index].getReference();
        while (curr != null) {
            if (curr.key > key) {
                newNode.next.set(curr.next);
                if (cas(table, index, curr, newNode)) {
                    return true;
                }
            }
            prev = curr;
            curr = curr.next.getReference();
        }
        if (prev != null) {
            prev.next.set(new AtomicMarkableReference<>(newNode, false));
            return true;
        }
        if (table[index].compareAndSet(null, newNode)) {
            return true;
        }
        return add(key);
    }

    @Override
    public boolean remove(int key) {
        int index = hash(key);
        Node prev = null;
        Node curr = table[index].getReference();
        while (curr != null) {
            if (curr.key == key) {
                if (curr.marked) {
                    return false;
                }
                AtomicMarkableReference<Node> next = curr.next;
                if (next.compareAndSet(curr.next.getReference(), curr.next.getReference(), false, true)) {
                    // Node has been marked
                    if (prev != null) {
                        prev.next.set(next);
                    } else {
                        table[index].set(next);
                    }
                    return true;
                }
            }
            prev = curr;
            curr = curr.next.getReference();
        }
        return false;
    }

    @Override
    public boolean contains(int key) {
        int index = hash(key);
        Node node = table[index].getReference();
        while (node != null) {
            if (node.key == key && !node.marked) {
                return true;
            }
            node = node.next.getReference();
        }
        return false;
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key, boolean marked) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, marked);
        }
    }
}