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
        Node tail = new Node(Integer.MAX_VALUE);
        head.next.set(tail, false);
    }

    public boolean add(int key) {
        while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = succ;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    if (curr.key == key) {
                        return false;
                    }
                    Node newNode = new Node(key);
                    newNode.next.set(curr, false);
                    if (pred.next.compareAndSet(curr, newNode, false, false)) {
                        return true;
                    }
                    break;
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    public boolean remove(int key) {
        while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = succ;
                    succ = curr.next.get(marked);
                }
                if (curr.key > key) {
                    return false;
                }
                if (curr.key == key) {
                    if (!curr.next.attemptMark(succ, true)) {
                        continue;
                    }
                    // Node has been marked
                    pred.next.compareAndSet(curr, succ, false, false);
                    return true;
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    public boolean contains(int key) {
        Node curr = head.next.getReference();
        while (curr.key < key) {
            curr = curr.next.getReference();
        }
        boolean[] marked = {false};
        curr.next.get(marked);
        return curr.key == key && !marked[0];
    }
}