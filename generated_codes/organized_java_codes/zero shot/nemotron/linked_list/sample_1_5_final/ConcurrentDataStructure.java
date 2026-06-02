package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        Node h = new Node(Integer.MIN_VALUE);
        Node t = new Node(Integer.MAX_VALUE);
        h.next.set(t, false);
        this.head = new AtomicReference<>(h);
    }

    @Override
    public boolean add(int key) {
        Node[] predCurr = new Node[2];
        while (true) {
            find(key, predCurr);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next.set(curr, false);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] predCurr = new Node[2];
        while (true) {
            find(key, predCurr);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            boolean marked = curr.next.attemptMark(succ, true);
            if (!marked) {
                return false;
            }
            // Node has been marked
            if (pred.next.compareAndSet(curr, succ, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = new Node[2];
        find(key, predCurr);
        Node pred = predCurr[0];
        Node curr = predCurr[1];
        return (curr.key == key) && !curr.next.isMarked();
    }

    private void find(int key, Node[] result) {
        RETRY:
        while (true) {
            Node pred = head.getReference();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean[] marked = {false};
                curr.next.get(marked);
                while (marked[0]) {
                    boolean snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip) {
                        continue RETRY;
                    }
                    curr = succ;
                    succ = curr.next.getReference();
                    curr.next.get(marked);
                }
                if (curr.key >= key) {
                    result[0] = pred;
                    result[1] = curr;
                    return;
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
}