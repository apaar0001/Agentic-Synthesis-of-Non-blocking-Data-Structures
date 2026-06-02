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

    /**
     * Returns [pred, curr] where pred.key < key <= curr.key and neither node is marked.
     */
    private Node[] find(int key) {
        boolean[] marked = {false};
        retry:
        while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference(marked);
                while (marked[0]) {
                    // Help remove curr
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        continue retry;
                    }
                    // Successfully removed curr, refetch succ and marked
                    succ = pred.next.getReference(marked);
                    // Continue checking if the new succ is also marked
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
            // else retry
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
            // Try to logically delete curr by marking its next pointer
            if (curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, false, false);
                return true;
            }
            // If CAS failed, retry to find again (possible concurrent modification)
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] predCurr = find(key);
        Node curr = predCurr[1];
        return curr.key == key;
    }
}