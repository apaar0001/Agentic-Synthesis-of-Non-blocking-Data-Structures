package com.example.Sets;
import com.example.utils.SetADT;
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

        Node() {
            this.key = 0;
            this.left = null;
            this.right = null;
            this.isLeaf = false;
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

    @Override
    public boolean add(int key) {
        while (true) {
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            while (true) {
                Node succ = curr.next.get(marked);
                while (marked[0]) {
                    Node succUnmarked = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, succUnmarked, false, false)) {
                        curr = succUnmarked;
                        if (curr != null) {
                            succ = curr.next.get(marked);
                        }
                    } else {
                        break;
                    }
                    if (!marked[0]) break;
                }
                if (curr == tail) break;
                if (curr.key >= key) break;
                pred = curr;
                curr = succ;
            }
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
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            while (true) {
                Node succ = curr.next.get(marked);
                while (marked[0]) {
                    Node succUnmarked = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, succUnmarked, false, false)) {
                        curr = succUnmarked;
                        if (curr != null) {
                            succ = curr.next.get(marked);
                        }
                    } else {
                        break;
                    }
                    if (!marked[0]) break;
                }
                if (curr == tail) break;
                if (curr.key >= key) break;
                pred = curr;
                curr = succ;
            }
            if (curr == tail || curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, false, false);
                return true;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            boolean valid = true;
            while (curr != tail) {
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    valid = false;
                    break;
                }
                if (curr.key == key) {
                    return true;
                }
                if (curr.key > key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            if (valid) {
                return false;
            }
        }
    }
}