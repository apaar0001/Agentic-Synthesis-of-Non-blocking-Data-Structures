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
        head.next.set(tail, false);
        tail.next.set(tail, false);
    }

    private boolean isMarked(Node node) {
        boolean[] marked = {false};
        node.next.get(marked);
        return marked[0];
    }

    private Node[] find(int key) {
        Node pred = null;
        Node curr = null;
        Node succ = null;
        boolean[] marked = {false};
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                if (curr == null) break;
                succ = curr.next.getReference();
                curr.next.get(marked);
                boolean currMarked = marked[0];
                if (currMarked) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            if (pred != null) pred.next.get(marked);
            if (!marked[0] && pred != null && pred.next.getReference() == curr) {
                return new Node[]{pred, curr};
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        Node curr = predCurr[1];
        return curr != null && curr.key == key && !isMarked(curr);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr != null && curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next.set(curr, false);
            if (pred != null && pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr == null || curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            boolean marked = curr.next.attemptMark(succ, true);
            if (marked) {
                if (pred != null) {
                    pred.next.compareAndSet(curr, succ, false, false);
                }
                return true;
            }
        }
    }
}