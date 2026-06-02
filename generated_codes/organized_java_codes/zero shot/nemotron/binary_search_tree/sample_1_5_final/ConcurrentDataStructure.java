package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        Node left;
        Node right;
        boolean isLeaf;
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.left = null;
            this.right = null;
            this.isLeaf = false;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE);
        head.next.set(new Node(Integer.MAX_VALUE), false);
    }

    private boolean isMarked(AtomicMarkableReference<Node> ref) {
        boolean[] marked = {false};
        ref.get(marked);
        return marked[0];
    }

    private Node[] find(int key) {
        boolean[] marked = {false};
        Node pred = null;
        Node curr = null;
        Node succ = null;
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                succ = curr.next.getReference();
                boolean[] markedSucc = {false};
                curr.next.get(markedSucc);
                while (markedSucc[0]) {
                    Node nextSucc = succ.next.getReference();
                    boolean[] markedNextSucc = {false};
                    succ.next.get(markedNextSucc);
                    if (pred.next.compareAndSet(curr, nextSucc, false, markedNextSucc[0])) {
                        curr = pred.next.getReference();
                        succ = curr.next.getReference();
                        curr.next.get(markedSucc);
                    } else {
                        break;
                    }
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
            Node curr = predCurr[1];
            if (curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            boolean[] marked = {false};
            pred.next.get(marked);
            if (marked[0]) {
                pred.next.compareAndSet(curr, succ, false, false);
                continue;
            }
            if (pred.next.attemptMark(curr, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, true, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        Node curr = predCurr[1];
        boolean[] marked = {false};
        predCurr[0].next.get(marked);
        return curr.key == key && !marked[0];
    }
}