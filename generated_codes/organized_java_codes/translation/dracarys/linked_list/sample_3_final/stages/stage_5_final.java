package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        Node dummy = new Node(Integer.MIN_VALUE);
        head = new AtomicReference<>(dummy);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node prev = head.get();
            Node curr = prev.next.getReference();
            while (curr != null && curr.key < key) {
                prev = curr;
                curr = curr.next.getReference();
            }
            if (curr != null && curr.key == key) {
                return false;
            }
            Node newNode = new Node(key);
            newNode.next.set(prev.next.getReference(), false);
            if (prev.next.compareAndSet(curr, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node prev = head.get();
            Node curr = prev.next.getReference();
            while (curr != null && curr.key < key) {
                prev = curr;
                curr = curr.next.getReference();
            }
            if (curr == null || curr.key != key) {
                return false;
            }
            Node next = curr.next.getReference();
            if (curr.next.compareAndSet(next, next, false, true)) {
                // Node has been marked
            }
            if (prev.next.compareAndSet(curr, next, false, false)) {
                return true;
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