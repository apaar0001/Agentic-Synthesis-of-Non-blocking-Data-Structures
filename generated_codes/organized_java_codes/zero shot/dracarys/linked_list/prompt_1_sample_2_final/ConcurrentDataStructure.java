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
                } else {
                    if (current.next.getReference() == next) {
                        if (next.marked.get()) {
                            if (current.next.compareAndSet(next, next.next.getReference())) {
                                continue;
                            }
                        } else {
                            if (current.data == key) {
                                return false;
                            }
                            current = next;
                        }
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
                if (current.next.compareAndSet(null, null)) {
                    head.compareAndSet(current, current.next.getReference());
                    return true;
                } else {
                    Node next = current.next.getReference();
                    if (next.marked.get()) {
                        if (current.next.compareAndSet(next, next.next.getReference())) {
                            continue;
                        }
                    } else {
                        if (current.next.compareAndSet(next, null)) {
                            head.compareAndSet(current, current.next.getReference());
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
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                        continue;
                    }
                } else {
                    if (next.data == key) {
                        if (next.marked.compareAndSet(false, true)) {
                            // Node has been marked
                            return true;
                        } else {
                            return false;
                        }
                    } else {
                        current = next;
                    }
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
                } else {
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    return false;
                }
                if (next.marked.get()) {
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                        continue;
                    }
                } else {
                    if (next.data == key) {
                        return true;
                    }
                    current = next;
                }
            }
        }
    }

    private class Node {
        int data;
        AtomicReference<Node> next;
        AtomicMarkableReference<Boolean> marked;

        Node(int data) {
            this.data = data;
            this.next = new AtomicReference<>(null);
            this.marked = new AtomicMarkableReference<>(false);
        }
    }
}