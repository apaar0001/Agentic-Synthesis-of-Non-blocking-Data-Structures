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

    private boolean isMarked(Node node) {
        return node.next.getBoolean();
    }

    private Node[] find(int key) {
        retry:
        while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.getBoolean();
                while (marked) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        continue retry;
                    }
                    curr = succ;
                    succ = curr.next.getReference();
                    marked = curr.next.getBoolean();
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
            Node[] res = find(key);
            Node pred = res[0];
            Node curr = res[1];
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
            Node[] res = find(key);
            Node pred = res[0];
            Node curr = res[1];
            if (curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] res = find(key);
        return res[1].key == key;
    }
}