package com.example.Sets;
import com.example.utils.SetADT;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.util.concurrent.atomic.AtomicBoolean;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final VarHandle VH;
    private final Node header;
    private final Node tail;
    private int level;

    static {
        try {
            VH = MethodHandles.arrayElementVarHandle(Node[].class);
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    public ConcurrentDataStructure() {
        level = 1;
        header = new Node(Integer.MIN_VALUE);
        tail = new Node(Integer.MAX_VALUE);
        header.forward[1] = tail;
    }

    private static class Node {
        int key;
        Node[] forward;
        final AtomicBoolean marked;

        Node(int key) {
            this.key = key;
            this.forward = new Node[MAX_LEVEL + 1];
            this.marked = new AtomicBoolean(false);
        }
    }

    private Node getNext(Node node, int idx) {
        return node.forward[idx];
    }

    private boolean casNext(Node pred, int idx, Node expect, Node update) {
        return VH.compareAndSet(pred.forward, idx, expect, update);
    }

    private Node[] find(int key) {
        Node pred = header;
        Node curr = getNext(pred, 1);
        while (true) {
            Node succ = getNext(curr, 1);
            boolean marked = curr.marked.get();
            while (marked) {
                if (casNext(pred, 1, curr, succ)) {
                    // helped
                } else {
                    break;
                }
                curr = getNext(pred, 1);
                succ = getNext(curr, 1);
                marked = curr.marked.get();
            }
            if (curr == tail || curr.key >= key) {
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
            if (curr != tail && curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.forward[1] = curr;
            if (casNext(pred, 1, curr, node)) {
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
            if (node == tail || node.key != key) {
                return false;
            }
            if (!node.marked.compareAndSet(false, true)) {
                return false;
            }
            // Node has been marked
            Node succ = node.forward[1];
            casNext(pred, 1, node, succ);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node pred = header;
        Node curr = getNext(pred, 1);
        while (true) {
            Node succ = getNext(curr, 1);
            boolean marked = curr.marked.get();
            while (marked) {
                if (casNext(pred, 1, curr, succ)) {
                    // helped
                } else {
                    break;
                }
                curr = getNext(pred, 1);
                succ = getNext(curr, 1);
                marked = curr.marked.get();
            }
            if (curr == tail || curr.key >= key) {
                return (curr != tail && curr.key == key && !curr.marked.get());
            }
            pred = curr;
            curr = succ;
        }
    }
}