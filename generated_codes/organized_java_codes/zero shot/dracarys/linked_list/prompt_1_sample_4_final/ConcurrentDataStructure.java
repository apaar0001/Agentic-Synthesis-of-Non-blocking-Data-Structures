package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        head = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        Node newNode = new Node(key);
        while (true) {
            Node current = head.get();
            Node next = current == null ? null : current.next.getReference();
            if (next == null) {
                if (current == null) {
                    if (head.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    if (current.next.compareAndSet(null, newNode)) {
                        return true;
                    }
                }
            } else {
                if (current.next.getReference() != next) {
                    continue;
                }
                if (next.data == key) {
                    return false;
                }
                if (next.marked.get()) {
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                        continue;
                    }
                }
            }
            head.set(current.next.getReference());
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = head.get();
            if (current == null) {
                return false;
            }
            Node next = current.next.getReference();
            if (next == null) {
                if (current.data == key) {
                    if (head.compareAndSet(current, null)) {
                        return true;
                    }
                } else {
                    return false;
                }
            } else {
                if (current.next.getReference() != next) {
                    continue;
                }
                if (next.data == key) {
                    if (next.marked.get()) {
                        return false;
                    }
                    if (next.marked.compareAndSet(false, true)) {
                        // Node has been marked
                        return true;
                    }
                } else {
                    if (next.marked.get()) {
                        if (current.next.compareAndSet(next, next.next.getReference())) {
                            continue;
                        }
                    }
                }
            }
            head.set(current.next.getReference());
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
                if (current.data == key) {
                    return true;
                }
                return false;
            } else {
                if (current.next.getReference() != next) {
                    continue;
                }
                if (next.marked.get()) {
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                        continue;
                    }
                } else {
                    if (next.data == key) {
                        return true;
                    }
                }
            }
            head.set(current.next.getReference());
        }
    }

    private class Node {
        int data;
        AtomicMarkableReference<Node> next;

        Node(int data) {
            this.data = data;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
}