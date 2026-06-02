```java
package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

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

    private static class Window {
        final Node pred;
        final Node curr;

        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }

    private Window find(int key) {
        Node pred;
        Node curr;
        Node succ;
        boolean[] marked = {false};
        boolean retry;

        retry_loop:
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        continue retry_loop;
                    }
                    curr = succ;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    return new Window(pred, curr);
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Window window = find(key);
            Node pred = window.pred;
            Node curr = window.curr;
            if (curr.key == key) {
                return false;
            } else {
                Node node = new Node(key);
                node.next.set(curr, false);
                if (pred.next.compareAndSet(curr, node, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Window window = find(key);
            Node pred = window.pred;
            Node curr = window.curr;
            if (curr.key != key) {
                return false;
            } else {
                Node succ = curr.next.getReference();
                if (curr.next.compareAndSet(succ, succ, false, true)) {
                    // Node has been marked
                    pred.next.compareAndSet(curr, succ, false, false);
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = {false};
        Node curr = head;
        while (curr.key < key) {
            curr = curr.next.getReference();
        }
        curr.next.get(marked);
        return curr.key == key && !marked[0];
    }
}

```