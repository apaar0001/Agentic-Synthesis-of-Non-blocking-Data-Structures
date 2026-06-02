package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final class Node {
        Integer key;
        AtomicMarkableReference<Node> left;
        Node right;
        boolean isLeaf;
        Node(Integer k) {
            this.key = k;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = null;
            this.isLeaf = false;
        }
    }

    private final Node head;
    private final Node tail;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE);
        tail = new Node(Integer.MAX_VALUE);
        head.left.set(tail, false);
        tail.left.set(null, false);
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
            node.left.set(curr, false);
            if (pred.left.compareAndSet(curr, node, false, false)) {
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
            Node succ = node.left.getReference();
            if (!node.left.attemptMark(succ, true)) {
                continue;
            }
            // Node has been marked
            pred.left.compareAndSet(node, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = head.left.getReference();
        while (curr != tail) {
            Node next = curr.left.getReference();
            boolean[] marked = {false};
            curr.left.get(marked);
            if (marked[0]) {
                curr = next;
                continue;
            }
            if (curr.key == key) {
                return true;
            } else if (curr.key > key) {
                return false;
            } else {
                curr = next;
            }
        }
        return false;
    }

    private Node[] find(int key) {
        boolean[] marked = {false};
        Node pred = null;
        Node curr = null;
        Node succ = null;
        retry:
        while (true) {
            pred = head;
            curr = pred.left.getReference();
            while (true) {
                succ = curr.left.getReference(marked);
                while (marked[0]) {
                    boolean snip = pred.left.compareAndSet(curr, succ, false, false);
                    if (!snip) {
                        continue retry;
                    }
                    curr = pred.left.getReference();
                    succ = curr.left.getReference(marked);
                }
                if (curr == tail) {
                    break retry;
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            return new Node[]{pred, curr};
        }
        return new Node[]{pred, curr};
    }
}