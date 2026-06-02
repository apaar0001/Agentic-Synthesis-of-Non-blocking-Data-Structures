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
    }

    private Node[] find(int key) {
        boolean[] marked = {false};
        outer:
        while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.get(marked);
                while (marked[0]) {
                    // Help remove curr
                    boolean snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip) {
                        // lost race, retry from start
                        break outer;
                    }
                    curr = pred.next.getReference();
                    if (curr == null) break; // should not happen
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
            node.next.set(curr, false);
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
            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, false, false);
                return true;
            }
            // else retry
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        Node curr = predCurr[1];
        return curr.key == key;
    }
}