package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int INITIAL_CAPACITY = 16;
    private static final float LOAD_FACTOR = 0.75f;

    private AtomicReference<Node>[] table;

    public ConcurrentDataStructure() {
        table = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < table.length; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return key % table.length;
    }

    private Node newNode(int key, Node next) {
        return new Node(key, next);
    }

    private Node findNode(Node head, int key) {
        Node prev = null;
        Node curr = head;
        while (curr != null) {
            if (curr.key == key) {
                return curr;
            }
            prev = curr;
            curr = curr.next.getReference();
        }
        return null;
    }

    private Node findPrevNode(Node head, int key) {
        Node prev = null;
        Node curr = head;
        while (curr != null) {
            if (curr.key == key) {
                return prev;
            }
            prev = curr;
            curr = curr.next.getReference();
        }
        return null;
    }

    private boolean add(int key) {
        int index = hash(key);
        Node head = table[index].getReference();
        Node prev = findPrevNode(head, key);
        if (prev != null) {
            return false;
        }
        Node newNode = newNode(key, head);
        if (table[index].compareAndSet(head, newNode)) {
            return true;
        }
        return add(key);
    }

    private boolean remove(int key) {
        int index = hash(key);
        Node head = table[index].getReference();
        Node prev = findPrevNode(head, key);
        if (prev == null) {
            return false;
        }
        Node curr = prev.next.getReference();
        if (curr.key == key) {
            if (prev.next.compareAndSet(curr, curr.next.getReference())) {
                curr.mark.set(true); // Node has been marked
                return true;
            }
        }
        return remove(key);
    }

    private boolean contains(int key) {
        int index = hash(key);
        Node head = table[index].getReference();
        Node curr = head;
        while (curr != null) {
            if (curr.key == key && !curr.mark.get()) {
                return true;
            }
            curr = curr.next.getReference();
        }
        return false;
    }

    @Override
    public boolean add(int key) {
        return add(key);
    }

    @Override
    public boolean remove(int key) {
        return remove(key);
    }

    @Override
    public boolean contains(int key) {
        return contains(key);
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        AtomicBoolean mark;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
            this.mark = new AtomicBoolean(false);
        }
    }
}