package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        head = new AtomicReference<>(new Node(Integer.MIN_VALUE, null));
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node prev = head.get();
            Node curr = prev.next.getReference();
            if (curr == null || curr.key >= key) {
                if (curr != null && curr.key == key) {
                    return false;
                }
                Node newNode = new Node(key, curr);
                if (prev.next.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            } else {
                if (prev.next.isMarked()) {
                    if (!prev.next.compareAndSet(curr, curr, true, true)) {
                        continue;
                    }
                }
                if (prev.next.getReference() != curr) {
                    continue;
                }
                if (curr.next.isMarked()) {
                    Node next = curr.next.getReference();
                    if (prev.next.compareAndSet(curr, next, false, false)) {
                        continue;
                    }
                }
                if (prev.next.getReference() != curr) {
                    continue;
                }
                if (curr.next.getReference() != null && curr.next.getReference().key < key) {
                    continue;
                }
                Node newNode = new Node(key, curr.next.getReference());
                if (curr.next.compareAndSet(curr.next.getReference(), newNode, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node prev = head.get();
            Node curr = prev.next.getReference();
            if (curr == null || curr.key > key) {
                return false;
            }
            if (curr.key < key) {
                if (curr.next.isMarked()) {
                    Node next = curr.next.getReference();
                    if (prev.next.compareAndSet(curr, next, false, false)) {
                        continue;
                    }
                }
                if (prev.next.getReference() != curr) {
                    continue;
                }
                if (curr.next.getReference() == null || curr.next.getReference().key >= key) {
                    continue;
                }
                prev = curr;
                curr = curr.next.getReference();
            }
            if (curr.key == key) {
                if (curr.next.isMarked()) {
                    return false;
                }
                Node next = curr.next.getReference();
                if (curr.next.compareAndSet(next, next, false, true)) {
                    // Node has been marked
                    if (prev.next.compareAndSet(curr, next, false, false)) {
                        return true;
                    }
                }
            } else {
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node curr = head.get();
            while (curr != null) {
                if (curr.next.isMarked()) {
                    curr = curr.next.getReference();
                    continue;
                }
                if (curr.key == key) {
                    return true;
                }
                if (curr.key > key) {
                    return false;
                }
                curr = curr.next.getReference();
            }
            return false;
        }
    }
}