package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
private static final class Node {
final int key;
final AtomicMarkableReference<Node> left;
final AtomicMarkableReference<Node> right;


    Node(int key, Node left) {
        this.key = key;
        this.left = new AtomicMarkableReference<>(left, false);
        this.right = new AtomicMarkableReference<>(null, false);
    }
}

private final AtomicMarkableReference<Node> root;

public ConcurrentDataStructure() {
    this.root = new AtomicMarkableReference<>(null, false);
}

@Override
public boolean add(int key) {
    while (true) {
        Window w = find(key);
        Node pred = w.pred;
        Node curr = w.curr;

        if (curr != null && curr.key == key) {
            return false;
        }

        Node node = new Node(key, curr);

        if (pred == null) {
            if (root.compareAndSet(curr, node, false, false)) {
                return true;
            }
        } else if (!pred.left.isMarked()) {
            if (pred.left.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }
}

@Override
public boolean remove(int key) {
    while (true) {
        Window w = find(key);
        Node pred = w.pred;
        Node curr = w.curr;

        if (curr == null || curr.key != key) {
            return false;
        }

        Node succ = curr.left.getReference();

        if (curr.left.compareAndSet(succ, succ, false, true)) {
            // Node has been marked
            unlink(pred, curr, succ);
            return true;
        }
    }
}

@Override
public boolean contains(int key) {
    Node curr = root.getReference();

    while (curr != null && curr.key > key) {
        curr = curr.left.getReference();
    }

    return curr != null && curr.key == key && !curr.left.isMarked();
}

private Window find(int key) {
    retry:
    while (true) {
        Node pred = null;
        Node curr = root.getReference();

        while (curr != null) {
            Node succ = curr.left.getReference();

            if (curr.left.isMarked()) {
                if (!unlink(pred, curr, succ)) {
                    continue retry;
                }
                curr = succ;
                continue;
            }

            if (curr.key <= key) {
                return new Window(pred, curr);
            }

            pred = curr;
            curr = succ;
        }

        return new Window(pred, null);
    }
}

private boolean unlink(Node pred, Node curr, Node succ) {
    if (pred == null) {
        return root.compareAndSet(curr, succ, false, false) || root.getReference() != curr;
    }

    if (pred.left.isMarked()) {
        return false;
    }

    return pred.left.compareAndSet(curr, succ, false, false) || pred.left.getReference() != curr;
}

private static final class Window {
    final Node pred;
    final Node curr;

    Window(Node pred, Node curr) {
        this.pred = pred;
        this.curr = curr;
    }
}


}
