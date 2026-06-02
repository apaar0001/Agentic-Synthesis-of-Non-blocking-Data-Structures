package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final AtomicReference<Integer> key;
        final AtomicMarkableReference<Node> next;
        final Node left;
        final Node right;
        final boolean isLeaf;

        Node(Integer key) {
            this.key = new AtomicReference<>(key);
            this.next = new AtomicMarkableReference<>(null, false);
            this.left = null;
            this.right = null;
            this.isLeaf = false;
        }

        Node() {
            this.key = new AtomicReference<>(null);
            this.next = new AtomicMarkableReference<>(null, false);
            this.left = null;
            this.right = null;
            this.isLeaf = false;
        }
    }

    private final Node head = new Node();

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];

            if (curr != null && curr.key.get() != null && curr.key.get() == key) {
                return false;
            }

            Node newNode = new Node(key);
            newNode.next.set(pred.next.getReference(), false);

            if (pred.next.compareAndSet(curr, newNode, false, false)) {
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

            if (curr == null || curr.key.get() == null || curr.key.get() != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            if (curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, false, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = find(key)[1];
        return curr != null && curr.key.get() != null && curr.key.get() == key;
    }

    private Node[] find(int key) {
        Node pred = head;
        Node curr = head.next.getReference();
        while (true) {
            if (curr == null) {
                return new Node[]{pred, null};
            }
            boolean[] marked = {false};
            Node succ = curr.next.get(marked);
            if (marked[0]) {
                pred.next.compareAndSet(curr, succ, false, false);
                curr = pred.next.getReference();
                continue;
            }
            Integer currKey = curr.key.get();
            if (currKey == null) {
                return new Node[]{pred, null};
            }
            if (currKey >= key) {
                return new Node[]{pred, curr};
            }
            pred = curr;
            curr = succ;
        }
    }
}