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

    boolean isMarked() {
        return right.isMarked();
    }
}

private final AtomicMarkableReference<Node> root;

public ConcurrentDataStructure() {
    this.root = new AtomicMarkableReference<>(null, false);
}

@Override
public boolean add(int key) {
    Node newNode = new Node(key);

    while (true) {
        Node r = root.getReference();
        if (r == null) {
            if (root.compareAndSet(null, newNode, false, false)) {
                return true;
            }
            continue;
        }

        Window w = find(key);
        Node parent = w.parent;
        Node curr = w.curr;

        if (curr != null && curr.key == key && !curr.isMarked()) {
            return false;
        }

        if (parent == null) {
            continue;
        }

        AtomicMarkableReference<Node> childRef = key < parent.key ? parent.left : parent.right;
        Node observed = childRef.getReference();

        if (observed == curr && !childRef.isMarked() && !parent.isMarked()) {
            if (childRef.compareAndSet(curr, newNode, false, false)) {
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

        if (curr == null || curr.key != key || curr.isMarked()) {
            return false;
        }

        Node rightChild = curr.right.getReference();
        if (curr.right.compareAndSet(rightChild, rightChild, false, true)) {
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
        if (curr.isMarked()) {
            curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
            continue;
        }

        if (key == curr.key) {
            return true;
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
            if (curr.isMarked()) {
                if (!helpUnlink(parent, curr)) {
                    continue retry;
                }

                curr = parent == null
                        ? root.getReference()
                        : (key < parent.key ? parent.left.getReference() : parent.right.getReference());
                continue;
            }

            if (key == curr.key) {
                return new Window(parent, curr);
            }

            parent = curr;
            curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
        }

        return new Window(parent, null);
    }
}

private boolean helpUnlink(Node parent, Node curr) {
    Node leftChild = curr.left.getReference();
    Node rightChild = curr.right.getReference();

    if (leftChild != null && rightChild != null) {
        return true;
    }

    Node replacement = leftChild != null ? leftChild : rightChild;

    if (parent == null) {
        return root.compareAndSet(curr, replacement, false, false) || root.getReference() != curr;
    }

    if (parent.isMarked()) {
        return false;
    }

    AtomicMarkableReference<Node> link = curr.key < parent.key ? parent.left : parent.right;
    return link.compareAndSet(curr, replacement, false, false) || link.getReference() != curr;
}

private static final class Window {
    final Node parent;
    final Node curr;

    Window(Node parent, Node curr) {
        this.parent = parent;
        this.curr = curr;
    }
}


}
