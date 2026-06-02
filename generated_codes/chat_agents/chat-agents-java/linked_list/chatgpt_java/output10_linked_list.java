package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
private final Node head;
private final Node tail;


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
    tail = new Node(Integer.MAX_VALUE, null);
    head = new Node(Integer.MIN_VALUE, tail);
}

private Window find(int key) {
    retry:
    while (true) {
        Node pred = head;
        Node curr = pred.next.getReference();

        while (true) {
            boolean[] marked = {false};
            Node succ = curr.next.get(marked);

            while (marked[0]) {
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    continue retry;
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
    if (key == Integer.MIN_VALUE || key == Integer.MAX_VALUE) {
        throw new IllegalArgumentException("Reserved key");
    }

    while (true) {
        Window window = find(key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr.key == key) {
            return false;
        }

        Node node = new Node(key, curr);
        if (pred.next.compareAndSet(curr, node, false, false)) {
            return true;
        }
    }
}

@Override
public boolean remove(int key) {
    if (key == Integer.MIN_VALUE || key == Integer.MAX_VALUE) {
        return false;
    }

    while (true) {
        Window window = find(key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr.key != key) {
            return false;
        }

        Node succ = curr.next.getReference();
        if (curr.next.compareAndSet(succ, succ, false, true)) {
            // Node has been marked
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }
}

@Override
public boolean contains(int key) {
    if (key == Integer.MIN_VALUE || key == Integer.MAX_VALUE) {
        return false;
    }

    Node curr = head.next.getReference();

    while (curr.key < key) {
        curr = curr.next.getReference();
    }

    return curr.key == key && !curr.next.isMarked();
}


}
