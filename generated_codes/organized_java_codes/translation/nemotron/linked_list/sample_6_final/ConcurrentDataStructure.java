package com.example.Sets;

import com.example.utils.SetADT;
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

    private final Node head;
    private final Node tail;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE);
        tail = new Node(Integer.MAX_VALUE);
        head.next = new AtomicMarkableReference<>(tail, false);
        tail.next = new AtomicMarkableReference<>(tail, false);
    }

    private Node[] find(int key) {
        Node pred = head;
        Node curr = pred.next.getReference();
        while (true) {
            Node succ = curr.next.getReference();
            boolean marked = curr.next.isMarked();
            if (marked) {
                // Help remove curr
                pred.next.compareAndSet(curr, succ, false, false);
                curr = pred.next.getReference();
                continue;
            }
            if (curr.key >= key) {
                return new Node[]{pred, curr};
            }
            pred = curr;
            curr = succ;
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next = new AtomicMarkableReference<>(curr, false);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
            // else retry
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key != key) {
                return false;
            }
            // Try to mark curr as logically deleted
            boolean marked = curr.next.attemptMark(curr.next.getReference(), true);
            if (!marked) {
                // Help removal of curr
                Node succ = curr.next.getReference();
                pred.next.compareAndSet(curr, succ, false, false);
                continue;
            }
            // Node has been marked
            // Now physically unlink
            Node succ = curr.next.getReference();
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        return predCurr[1].key == key;
    }
}