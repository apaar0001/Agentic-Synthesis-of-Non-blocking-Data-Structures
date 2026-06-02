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
        tail = new Node(Integer.MAX_VALUE);
        head = new Node(Integer.MIN_VALUE);
        head.next.set(tail, false);
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
                succ = curr.next.getReference(marked);
                while (marked[0]) {
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                    } else {
                        break;
                    }
                    curr = pred.next.getReference();
                    if (curr == tail) break;
                    succ = curr.next.getReference(marked);
                }
                if (curr == tail) break;
                if (!marked[0] && curr.key >= key) {
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
            if (curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        Node curr = predCurr[1];
        return curr.key == key && !curr.next.isMarked();
    }
}