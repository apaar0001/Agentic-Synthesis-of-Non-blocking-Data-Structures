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
        Node minNode = new Node(Integer.MIN_VALUE);
        Node maxNode = new Node(Integer.MAX_VALUE);
        minNode.next.set(maxNode, false);
        head.set(minNode);
    }

    private Node[] find(int key) {
        retry:
        while (true) {
            Node pred = head.get();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                while (marked) {
                    Node succNext = succ.next.getReference();
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        // helped remove curr
                    } else {
                        continue retry;
                    }
                    curr = succ;
                    succ = succNext;
                    marked = succ.next.isMarked();
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
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node node = predCurr[1];
            Node succ = node.next.getReference();
            if (node.key != key) {
                return false;
            }
            if (node.next.compareAndSet(node, succ, false, true)) {
                // Node has been marked
                pred.next.compareAndSet(node, succ, false, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = head.get();
        while (true) {
            Node succ = curr.next.getReference();
            boolean marked = curr.next.isMarked();
            while (marked) {
                curr = succ;
                succ = curr.next.getReference();
                marked = curr.next.isMarked();
            }
            if (curr.key >= key) {
                return curr.key == key;
            }
            curr = succ;
        }
    }
}