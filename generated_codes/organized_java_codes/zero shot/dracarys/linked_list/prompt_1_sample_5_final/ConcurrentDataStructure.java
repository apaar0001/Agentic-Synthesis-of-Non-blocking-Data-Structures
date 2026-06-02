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
            if (current == null) {
                if (head.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    if (current.next.compareAndSet(next, newNode)) {
                        return true;
                    }
                } else if (next.marked.get()) {
                    helpRemove(current, next);
                } else {
                    if (current.data == key) {
                        return false;
                    }
                    if (current.next.compareAndSet(next, newNode)) {
                        newNode.next.set(next);
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
                if (current.next.getReference() == null) {
                    if (head.compareAndSet(current, null)) {
                        return true;
                    }
                } else {
                    AtomicMarkableReference<Node> next = current.next;
                    if (next.compareAndSet(next.getReference(), next.getReference(), false, true)) {
                        // Node has been marked
                        return true;
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
                    if (current.next.compareAndSet(next, next.next.getReference())) {
                        next.marked.set(true);
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
                return true;
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.marked.get()) {
                helpRemove(current, next);
            } else if (next.data == key) {
                return true;
            } else {
                current = next;
            }
        }
    }

    private void helpRemove(Node prev, Node node) {
        if (prev != null) {
            prev.next.compareAndSet(node, node.next.getReference());
        } else {
            head.compareAndSet(node, node.next.getReference());
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