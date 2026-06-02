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
    }

    private Node[] find(int key) {
        Node pred = null;
        Node curr = null;
        Node succ = null;
        boolean[] marked = {false};
        boolean snip;
        retry:
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip) continue retry;
                    curr = pred.next.getReference();
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    return new Node[]{pred, curr};
                }
                pred = curr;
                curr = succ;
            }
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
        }
    }

    @Override
    public boolean remove(int key) {
        boolean snip;
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            snip = curr.next.attemptMark(succ, true);
            if (!snip) {
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
        while (true) {
            if (curr == null) {
                return false;
            }
            boolean marked = curr.next.isMarked();
            if (marked) {
                curr = curr.next.getReference();
                continue;
            }
            if (curr.key >= key) {
                break;
            }
            curr = curr.next.getReference();
        }
        return curr.key == key && !curr.next.isMarked();
    }
}