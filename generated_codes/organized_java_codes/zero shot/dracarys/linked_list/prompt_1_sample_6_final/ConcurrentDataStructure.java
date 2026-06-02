package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        head = new AtomicReference<>();
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node current = head.get();
            if (current != null && current.data == key) {
                return false;
            }
            Node newNode = new Node(key);
            if (head.compareAndSet(current, newNode)) {
                return true;
            }
            if (current != null) {
                Node next = current.next.getReference();
                if (next != null && next.data == key) {
                    return false;
                }
                if (current.next.compareAndSet(next, newNode)) {
                    return true;
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
                if (current.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next != null && next.data == key) {
                    if (next.marked.get()) {
                        return false;
                    }
                    if (next.marked.compareAndSet(false, true)) {
                        // Node has been marked
                        return true;
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
                }
                return true;
            } else {
                Node next = current.next.getReference();
                if (next != null && next.data == key) {
                    if (next.marked.get()) {
                        return false;
                    }
                    return true;
                }
            }
        }
    }

    private class Node {
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