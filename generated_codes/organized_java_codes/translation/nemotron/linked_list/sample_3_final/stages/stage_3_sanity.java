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

    @Override
    public boolean add(int key) {
        while (true) {
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            Node succ;

            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    Node nextUnmarked = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, nextUnmarked, false, false)) {
                        // helped unlink
                    } else {
                        break;
                    }
                    curr = nextUnmarked;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    break;
                }
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
            // else retry
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            Node succ;

            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    Node nextUnmarked = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, nextUnmarked, false, false)) {
                        // helped unlink
                    } else {
                        break;
                    }
                    curr = nextUnmarked;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }

            if (curr.key != key) {
                return false;
            }

            Node next = curr.next.getReference();
            boolean markedSuccess;
            do {
                markedSuccess = curr.next.attemptMark(next, true);
                if (!markedSuccess) {
                    next = curr.next.getReference();
                }
            } while (!markedSuccess);

            // Node has been marked
            // Attempt physical removal (optional for lock-freedom)
            pred.next.compareAndSet(curr, next, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = head.next.getReference();
        boolean[] marked = {false};
        Node succ;

        while (true) {
            succ = curr.next.get(marked);
            while (marked[0]) {
                curr = succ;
                succ = curr.next.get(marked);
            }
            if (curr.key >= key) {
                return curr.key == key && !marked[0];
            }
            curr = succ;
        }
    }
}