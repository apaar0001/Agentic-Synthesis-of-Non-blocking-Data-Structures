package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> head = new AtomicReference<>(null);

    private void find(int key, AtomicReference<Node> predRef, AtomicReference<Node> currRef) {
        retry:
        while (true) {
            Node pred = head.get();
            Node curr = (pred != null) ? pred.next.getReference() : null;
            while (true) {
                Node succ = (curr != null) ? curr.next.getReference() : null;
                boolean[] marked = {false};
                if (curr != null) {
                    curr.next.get(marked);
                }
                boolean currMarked = curr != null && marked[0];
                if (curr != null && currMarked) {
                    boolean snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip) {
                        continue retry;
                    }
                    curr = pred.next.getReference();
                    if (curr == null) break;
                    continue;
                }
                if (curr == null || curr.key >= key) {
                    predRef.set(pred);
                    currRef.set(curr);
                    return;
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    public boolean add(int key) {
        while (true) {
            AtomicReference<Node> pred = new AtomicReference<>();
            AtomicReference<Node> curr = new AtomicReference<>();
            find(key, pred, curr);
            Node c = curr.get();
            if (c != null && c.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next.set(c, false);
            Node p = pred.get();
            if (p == null) {
                if (head.compareAndSet(null, node)) {
                    return true;
                }
            } else {
                if (p.next.compareAndSet(c, node, false, false)) {
                    return true;
                }
            }
        }
    }

    public boolean remove(int key) {
        while (true) {
            AtomicReference<Node> pred = new AtomicReference<>();
            AtomicReference<Node> curr = new AtomicReference<>();
            find(key, pred, curr);
            Node p = pred.get();
            Node c = curr.get();
            if (c == null || c.key != key) {
                return false;
            }
            boolean[] marked = {false};
            c.next.get(marked);
            if (marked[0]) {
                return false;
            }
            Node succ = c.next.getReference();
            if (c.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                if (p == null) {
                    head.compareAndSet(c, succ);
                } else {
                    p.next.compareAndSet(c, succ, false, false);
                }
                return true;
            }
        }
    }

    public boolean contains(int key) {
        AtomicReference<Node> pred = new AtomicReference<>();
        AtomicReference<Node> curr = new AtomicReference<>();
        find(key, pred, curr);
        Node c = curr.get();
        if (c != null && c.key == key) {
            boolean[] marked = {false};
            c.next.get(marked);
            return !marked[0];
        }
        return false;
    }
}