package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
private final AtomicMarkableReference<Node> head;


private static final class Node {
    final int key;
    final AtomicMarkableReference<Node> next;

    Node(int key, Node next) {
        this.key = key;
        this.next = new AtomicMarkableReference<>(next, false);
    }
}

private static final class Window {
    final Node pred;
    final Node curr;

    Window(Node pred, Node curr) {
        this.pred = pred;
        this.curr = curr;
    }
}

public ConcurrentDataStructure() {
    this.head = new AtomicMarkableReference<>(null, false);
}

private Window find(int key) {
    retry:
    while (true) {
        Node pred = null;
        Node curr = head.getReference();

        while (curr != null) {
            boolean[] marked = {false};
            Node succ = curr.next.get(marked);

            while (marked[0]) {
                boolean unlinked;
                if (pred == null) {
                    unlinked = head.compareAndSet(curr, succ, false, false);
                } else {
                    unlinked = pred.next.compareAndSet(curr, succ, false, false);
                }

                if (!unlinked) {
                    continue retry;
                }

                curr = succ;
                if (curr == null) {
                    break;
                }
                succ = curr.next.get(marked);
            }

            if (curr == null || curr.key >= key) {
                return new Window(pred, curr);
            }

            pred = curr;
            curr = succ;
        }

        return new Window(pred, null);
    }
}

@Override
public boolean add(int key) {
    while (true) {
        Window window = find(key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr != null && curr.key == key) {
            return false;
        }

        Node node = new Node(key, curr);

        if (pred == null) {
            if (head.compareAndSet(curr, node, false, false)) {
                return true;
            }
        } else {
            if (!pred.next.isMarked() && pred.next.compareAndSet(curr, node, false, false)) {
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

        if (curr == null || curr.key != key) {
            return false;
        }

        Node succ = curr.next.getReference();
        if (curr.next.compareAndSet(succ, succ, false, true)) {
            // Node has been marked
            if (pred == null) {
                head.compareAndSet(curr, succ, false, false);
            } else {
                pred.next.compareAndSet(curr, succ, false, false);
            }
            return true;
        }
    }
}

@Override
public boolean contains(int key) {
    Window window = find(key);
    Node curr = window.curr;
    return curr != null && curr.key == key && !curr.next.isMarked();
}


}
