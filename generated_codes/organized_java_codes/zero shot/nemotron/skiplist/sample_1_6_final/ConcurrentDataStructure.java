package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static class Node {
        int key;
        final AtomicMarkableReference<Node>[] forward;
        Node(int key) {
            this.key = key;
            forward = new AtomicMarkableReference[MAX_LEVEL];
            for (int i = 0; i < MAX_LEVEL; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    private final Node header = new Node(Integer.MIN_VALUE);

    public ConcurrentDataStructure() {
    }

    private Node[] find(int key) {
        Node pred = header;
        Node curr;
        Node succ;
        while (true) {
            pred = header;
            curr = pred.forward[0].getReference();
            while (true) {
                succ = curr.forward[0].getReference();
                boolean marked = curr.forward[0].isMarked();
                if (marked) {
                    boolean snip = pred.forward[0].compareAndSet(curr, succ, false, false);
                    if (!snip) {
                        break;
                    }
                    curr = pred.forward[0].getReference();
                    succ = curr.forward[0].getReference();
                    marked = curr.forward[0].isMarked();
                    continue;
                }
                if (curr == null || curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            if (curr != null && !curr.forward[0].isMarked() && curr.key == key) {
                return new Node[]{pred, curr};
            }
            if (curr == null || curr.key > key) {
                return new Node[]{pred, curr};
            }
        }
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
            node.forward[0].set(curr, false);
            if (pred.forward[0].compareAndSet(curr, node, false, false)) {
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
            Node next = curr.forward[0].getReference();
            boolean snip = curr.forward[0].compareAndSet(next, next, false, true);
            if (!snip) {
                continue;
            }
            // Node has been marked
            pred.forward[0].compareAndSet(curr, next, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        Node curr = predCurr[1];
        return curr != null && curr.key == key && !curr.forward[0].isMarked();
    }
}