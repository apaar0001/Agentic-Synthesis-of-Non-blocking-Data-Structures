package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
private static final class Node {
final int key;
final AtomicMarkableReference<Node> left;
final AtomicMarkableReference<Node> right;


    Node(int key) {
        this.key = key;
        this.left = new AtomicMarkableReference<>(null, false);
        this.right = new AtomicMarkableReference<>(null, false);
    }

    boolean marked() {
        return right.isMarked();
    }
}

private static final class Window {
    final Node parent;
    final Node curr;

    Window(Node parent, Node curr) {
        this.parent = parent;
        this.curr = curr;
    }
}

private final AtomicMarkableReference<Node> root;

public ConcurrentDataStructure() {
    this.root = new AtomicMarkableReference<>(null, false);
}

@Override
public boolean add(int key) {
    while (true) {
        Node r = root.getReference();
        if (r == null) {
            if (root.compareAndSet(null, new Node(key), false, false)) {
                return true;
            }
            continue;
        }

        Window w = find(key);
        Node parent = w.parent;
        Node curr = w.curr;

        if (curr != null && curr.key == key && !curr.marked()) {
            return false;
        }

        if (parent == null || parent.marked()) {
            continue;
        }

        AtomicMarkableReference<Node> link = key < parent.key ? parent.left : parent.right;
        Node observed = link.getReference();

        if (observed == curr && !link.isMarked() && !parent.marked()) {
            if (link.compareAndSet(curr, new Node(key), false, false)) {
                return true;
            }
        }
    }
}

@Override
public boolean remove(int key) {
    while (true) {
        Window w = find(key);
        Node parent = w.parent;
        Node curr = w.curr;

        if (curr == null || curr.key != key || curr.marked()) {
            return false;
        }

        Node right = curr.right.getReference();
        if (curr.right.compareAndSet(right, right, false, true)) {
            // Node has been marked
            helpUnlink(parent, curr);
            return true;
        }
    }
}

@Override
public boolean contains(int key) {
    Node curr = root.getReference();

    while (curr != null) {
        if (key == curr.key) {
            return !curr.marked();
        }

        curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
    }

    return false;
}

private Window find(int key) {
    retry:
    while (true) {
        Node parent = null;
        Node curr = root.getReference();

        while (curr != null) {
            if (curr.marked()) {
                boolean helped = helpUnlink(parent, curr);
                if (!helped) {
                    if (curr.left.getReference() == null || curr.right.getReference() == null) {
                        continue retry;
                    }
                }
            }

            if (key == curr.key) {
                return new Window(parent, curr);
            }

            Node next = key < curr.key ? curr.left.getReference() : curr.right.getReference();

            if (!curr.marked()) {
                parent = curr;
            }

            curr = next;
        }

        return new Window(parent, null);
    }
}

private boolean helpUnlink(Node parent, Node curr) {
    if (curr == null || !curr.marked()) {
        return true;
    }

    Node left = curr.left.getReference();
    Node right = curr.right.getReference();

    if (left != null && right != null) {
        return false;
    }

    Node replacement = left != null ? left : right;

    if (parent == null) {
        return root.compareAndSet(curr, replacement, false, false) || root.getReference() != curr;
    }

    if (parent.marked()) {
        return false;
    }

    AtomicMarkableReference<Node> link = curr.key < parent.key ? parent.left : parent.right;
    return link.compareAndSet(curr, replacement, false, false) || link.getReference() != curr;
}


}
