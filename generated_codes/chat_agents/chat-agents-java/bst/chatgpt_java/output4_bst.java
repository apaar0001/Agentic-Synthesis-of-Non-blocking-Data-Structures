package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

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

private static final class SearchResult {
    final Node parent;
    final Node node;

    SearchResult(Node parent, Node node) {
        this.parent = parent;
        this.node = node;
    }
}

private final AtomicReference<Node> root;

public ConcurrentDataStructure() {
    this.root = new AtomicReference<>(null);
}

@Override
public boolean add(int key) {
    while (true) {
        Node r = root.get();
        if (r == null) {
            if (root.compareAndSet(null, new Node(key))) {
                return true;
            }
            continue;
        }

        SearchResult result = search(key);
        Node parent = result.parent;
        Node node = result.node;

        if (node != null && node.key == key) {
            if (!node.isMarked()) {
                return false;
            }

            Node right = node.right.getReference();
            if (node.right.compareAndSet(right, right, true, false)) {
                return true;
            }
            continue;
        }

        if (parent == null || parent.isMarked()) {
            continue;
        }

        Node newNode = new Node(key);
        AtomicMarkableReference<Node> childRef = key < parent.key ? parent.left : parent.right;
        Node observed = childRef.getReference();

        if (observed == null && !childRef.isMarked()) {
            if (childRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        } else {
            helpMarked(observed, parent);
        }
    }
}

@Override
public boolean remove(int key) {
    while (true) {
        SearchResult result = search(key);
        Node parent = result.parent;
        Node node = result.node;

        if (node == null || node.key != key || node.isMarked()) {
            return false;
        }

        Node right = node.right.getReference();
        if (node.right.compareAndSet(right, right, false, true)) {
            // Node has been marked
            helpMarked(node, parent);
            return true;
        }
    }
}

@Override
public boolean contains(int key) {
    Node curr = root.get();

    while (curr != null) {
        if (key == curr.key) {
            return !curr.isMarked();
        }

        curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
    }

    return false;
}

private SearchResult search(int key) {
    retry:
    while (true) {
        Node parent = null;
        Node curr = root.get();

        while (curr != null) {
            if (curr.isMarked()) {
                helpMarked(curr, parent);
            }

            if (key == curr.key) {
                return new SearchResult(parent, curr);
            }

            Node next = key < curr.key ? curr.left.getReference() : curr.right.getReference();

            if (curr.isMarked() && next == null && hasAtMostOneChild(curr)) {
                continue retry;
            }

            parent = curr;
            curr = next;
        }

        return new SearchResult(parent, null);
    }
}

private boolean hasAtMostOneChild(Node node) {
    return node.left.getReference() == null || node.right.getReference() == null;
}

private void helpMarked(Node node, Node parent) {
    if (node == null || !node.isMarked()) {
        return;
    }

    Node left = node.left.getReference();
    Node right = node.right.getReference();

    if (left != null && right != null) {
        return;
    }

    Node replacement = left != null ? left : right;

    if (parent == null) {
        root.compareAndSet(node, replacement);
        return;
    }

    if (parent.isMarked()) {
        return;
    }

    AtomicMarkableReference<Node> childRef = node.key < parent.key ? parent.left : parent.right;
    childRef.compareAndSet(node, replacement, false, false);
}


}
