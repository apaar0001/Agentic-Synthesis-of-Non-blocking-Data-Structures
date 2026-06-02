package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

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
            Node next = curr.next.getReference();

            if (curr.key >= key) {
                if (curr.key == key) {
                    return false;
                }
                Node newNode = new Node(key, curr);
                if (prev.next.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            } else {
                if (next == null || next.key >= key) {
                    Node newNode = new Node(key, next);
                    if (curr.next.compareAndSet(next, newNode, false, false)) {
                        return true;
                    }
                } else {
                    while (curr.next.isMarked()) {
                        curr = curr.next.getReference();
                    }
                    if (prev.next.compareAndSet(curr, curr.next.getReference(), false, false)) {
                        continue;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node prev = head.get();
            Node curr = prev.next.getReference();

            while (curr != null && (curr.key < key || curr.next.isMarked())) {
                prev = curr;
                curr = curr.next.getReference();
            }

            if (curr == null || curr.key != key) {
                return false;
            }

            Node next = curr.next.getReference();
            if (curr.next.compareAndSet(next, next, false, true)) {
                // Node has been marked
                if (prev.next.compareAndSet(curr, next, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = head.get();
        while (curr != null) {
            if (curr.key == key && !curr.next.isMarked()) {
                return true;
            }
            curr = curr.next.getReference();
        }
        return false;
    }
}