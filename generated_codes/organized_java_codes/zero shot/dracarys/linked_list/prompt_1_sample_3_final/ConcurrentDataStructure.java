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
            Node prev = null;
            Node curr = head.get();
            while (curr != null) {
                if (curr.data == key) {
                    return false;
                }
                if (curr.marked.get()) {
                    Node next = curr.next.get();
                    if (prev != null && prev.next.compareAndSet(curr, next)) {
                        curr = next;
                    } else {
                        break;
                    }
                } else {
                    prev = curr;
                    curr = curr.next.get();
                }
            }
            Node newNode = new Node(key);
            newNode.next.set(curr);
            if (prev == null) {
                if (head.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(curr, newNode)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node prev = null;
            Node curr = head.get();
            while (curr != null) {
                if (curr.marked.get()) {
                    Node next = curr.next.get();
                    if (prev != null && prev.next.compareAndSet(curr, next)) {
                        curr = next;
                    } else {
                        break;
                    }
                } else if (curr.data == key) {
                    if (curr.marked.attemptMark()) {
                        if (prev != null) {
                            prev.next.compareAndSet(curr, curr.next.get());
                        } else {
                            head.compareAndSet(curr, curr.next.get());
                        }
                        // Node has been marked
                        return true;
                    } else {
                        break;
                    }
                } else {
                    prev = curr;
                    curr = curr.next.get();
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node curr = head.get();
            while (curr != null) {
                if (curr.marked.get()) {
                    Node next = curr.next.get();
                    if (curr.next.compareAndSet(curr, next)) {
                        curr = next;
                    } else {
                        break;
                    }
                } else if (curr.data == key) {
                    return true;
                } else {
                    curr = curr.next.get();
                }
            }
            return false;
        }
    }

    private static class Node {
        int data;
        AtomicMarkableReference<Node> marked;
        AtomicReference<Node> next;

        Node(int data) {
            this.data = data;
            this.marked = new AtomicMarkableReference<>(false);
            this.next = new AtomicReference<>(null);
        }
    }
}