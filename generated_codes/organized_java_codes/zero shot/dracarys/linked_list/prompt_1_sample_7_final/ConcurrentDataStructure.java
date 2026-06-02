package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        this.head = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node current = head.get();
            Node newNode = new Node(key);
            if (current == null) {
                if (head.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    if (current.next.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    Node prev = current;
                    while (next != null) {
                        if (next.data == key) {
                            return false;
                        }
                        if (next.marked.get()) {
                            if (prev.next.compareAndSet(next, next.next.getReference())) {
                            }
                            next = prev.next.getReference();
                        } else {
                            prev = next;
                            next = next.next.getReference();
                        }
                    }
                    newNode.next.set(prev.next.getReference());
                    if (prev.next.compareAndSet(next, newNode)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = head.get();
            if (current == null) {
                return false;
            }
            if (current.data == key) {
                if (current.marked.get()) {
                    return false;
                }
                if (head.compareAndSet(current, current.next.getReference())) {
                    current.marked.set(true);
                    // Node has been marked
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    return false;
                }
                if (next.marked.get()) {
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                    }
                    continue;
                }
                if (next.data == key) {
                    if (next.marked.get()) {
                        return false;
                    }
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                        next.marked.set(true);
                        // Node has been marked
                        return true;
                    }
                } else {
                    current = next;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node current = head.get();
            if (current == null) {
                return false;
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.marked.get()) {
                if (current.next.compareAndSet(next, next.next.getReference())) {
                }
                continue;
            }
            if (next.data == key) {
                return true;
            }
            current = next;
        }
    }

    private static class Node {
        int data;
        AtomicMarkableReference<Node> next;
        AtomicBoolean marked;

        Node(int data) {
            this.data = data;
            this.next = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicBoolean(false);
        }
    }
}