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
                } else if (next.marked.get()) {
                    helpRemove(next);
                    continue;
                } else {
                    Node prev = findPrevious(current, next);
                    if (prev == null) {
                        continue;
                    }
                    if (prev.next.compareAndSet(next, newNode)) {
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
                    helpRemove(next);
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
                    Node prev = findPrevious(current, next);
                    if (prev == null) {
                        continue;
                    }
                    next = prev.next.getReference();
                    if (next.marked.get()) {
                        helpRemove(next);
                        continue;
                    }
                    if (next.data == key) {
                        if (next.marked.get()) {
                            return false;
                        }
                        if (prev.next.compareAndSet(next, next.next.getReference())) {
                            next.marked.set(true);
                            // Node has been marked
                            return true;
                        }
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
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.marked.get()) {
                helpRemove(next);
                continue;
            }
            if (next.data == key) {
                if (next.marked.get()) {
                    return false;
                }
                return true;
            }
            Node prev = findPrevious(current, next);
            if (prev == null) {
                continue;
            }
            next = prev.next.getReference();
            if (next.marked.get()) {
                helpRemove(next);
                continue;
            }
            if (next.data == key) {
                if (next.marked.get()) {
                    return false;
                }
                return true;
            }
        }
    }

    private Node findPrevious(Node current, Node next) {
        while (true) {
            if (current.next.getReference() == next) {
                return current;
            }
            current = current.next.getReference();
            if (current == null) {
                return null;
            }
        }
    }

    private void helpRemove(Node node) {
        while (true) {
            Node next = node.next.getReference();
            if (next == null) {
                return;
            }
            if (node.marked.get()) {
                if (node.next.compareAndSet(next, next.next.getReference())) {
                    return;
                }
            } else {
                return;
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