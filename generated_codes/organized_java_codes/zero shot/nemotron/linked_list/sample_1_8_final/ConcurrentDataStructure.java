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
        Node tail = new Node(Integer.MAX_VALUE);
        Node headNode = new Node(Integer.MIN_VALUE);
        headNode.next.set(tail, false);
        this.head = new AtomicReference<>(headNode);
    }

    private Node[] find(int key) {
        retry:
        while (true) {
            boolean[] marked = {false};
            Node pred = head.getReference();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        curr = pred.next.getReference();
                        continue;
                    } else {
                        continue retry;
                    }
                }
                if (curr.key >= key) {
                    return new Node[]{pred, curr};
                }
                pred = curr;
                curr = curr.next.getReference();
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
            if (pred.next.compareAndSet(curr, succ, false, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, true, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        Node pred = predCurr[0];
        Node curr = predCurr[1];
        return curr.key == key && pred.next.getReference() == curr && !pred.next.isMarked();
    }
}