package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE);
        head.next.set(new Node(Integer.MAX_VALUE), false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Window window = find(key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr.key == key) {
                return false;
            }

            Node newNode = new Node(key);
            newNode.next.set(curr, false);
            if (pred.next.compareAndSet(curr, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Window window = find(key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            if (!curr.next.compareAndSet(succ, succ, false, true)) {
                continue;
            }
            // Node has been marked
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = head.next.getReference();
        while (curr.key < key) {
            curr = curr.next.getReference();
        }
        return curr.key == key && !curr.next.isMarked();
    }

    private class Window {
        Node pred;
        Node curr;
    }

    private Window find(int key) {
        retry: while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean[] marked = {false};
                Node succRef = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succRef, false, false)) {
                        continue retry;
                    }
                    curr = succRef;
                    succRef = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    Window window = new Window();
                    window.pred = pred;
                    window.curr = curr;
                    return window;
                }
                pred = curr;
                curr = succRef;
            }
        }
    }
}