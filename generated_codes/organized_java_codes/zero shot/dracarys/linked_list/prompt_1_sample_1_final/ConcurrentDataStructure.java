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
            Node next = current != null ? current.next.getReference() : null;
            if (current != null && current.marked.get()) {
                helpDelete(current);
                continue;
            }
            if (next != null && next.marked.get()) {
                helpDelete(next);
                continue;
            }
            if (current != null && current.data == key) {
                return false;
            }
            if (current == null) {
                if (head.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (current.next.compareAndSet(next, newNode)) {
                    newNode.next.set(current.next.getReference());
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
            if (current.marked.get()) {
                helpDelete(current);
                continue;
            }
            if (current.data == key) {
                if (current.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    return true;
                } else {
                    return false;
                }
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.marked.get()) {
                helpDelete(next);
                continue;
            }
            if (next.data == key) {
                if (next.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                        return true;
                    }
                } else {
                    return false;
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
            if (current.marked.get()) {
                helpDelete(current);
                continue;
            }
            if (current.data == key) {
                return true;
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.marked.get()) {
                helpDelete(next);
                continue;
            }
            if (next.data == key) {
                return true;
            }
        }
    }

    private void helpDelete(Node node) {
        while (true) {
            Node next = node.next.getReference();
            if (next == null) {
                break;
            }
            if (node.next.compareAndSet(next, next.next.getReference())) {
                break;
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