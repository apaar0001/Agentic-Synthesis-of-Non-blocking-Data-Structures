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
        while (true) {
            Node current = head.get();
            if (current == null) {
                Node newNode = new Node(key);
                if (head.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    Node newNode = new Node(key);
                    if (current.next.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else if (next.marked.get()) {
                    helpRemove(current, next);
                } else {
                    current = next;
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
                if (current.next.getReference() == null) {
                    if (head.compareAndSet(current, null)) {
                        return true;
                    }
                } else {
                    Node next = current.next.getReference();
                    if (next.marked.get()) {
                        helpRemove(current, next);
                    } else {
                        if (current.next.compareAndSet(next, next.next.getReference())) {
                            current.marked.set(true);
                            // Node has been marked
                            return true;
                        }
                    }
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    return false;
                }
                if (next.marked.get()) {
                    helpRemove(current, next);
                } else if (next.data == key) {
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
            if (current.data == key) {
                if (current.marked.get()) {
                    return false;
                }
                return true;
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.marked.get()) {
                helpRemove(current, next);
            } else {
                current = next;
            }
        }
    }

    private void helpRemove(Node current, Node next) {
        if (next.marked.get()) {
            if (current.next.compareAndSet(next, next.next.getReference())) {
            }
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