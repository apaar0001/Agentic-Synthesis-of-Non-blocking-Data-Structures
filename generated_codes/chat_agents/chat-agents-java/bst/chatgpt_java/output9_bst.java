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
        final Node curr;

        SearchResult(Node parent, Node curr) {
            this.parent = parent;
            this.curr = curr;
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

            SearchResult result = find(key);
            Node parent = result.parent;
            Node curr = result.curr;

            if (curr != null) {
                if (!curr.isMarked()) {
                    return false;
                }
                continue;
            }

            if (parent == null || parent.isMarked()) {
                continue;
            }

            Node newNode = new Node(key);

            if (key < parent.key) {
                if (parent.left.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            } else {
                if (parent.right.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            SearchResult result = find(key);
            Node parent = result.parent;
            Node curr = result.curr;

            if (curr == null || curr.isMarked()) {
                return false;
            }

            Node rightRef = curr.right.getReference();

            if (curr.right.compareAndSet(rightRef, rightRef, false, true)) {
                // Node has been marked
                helpDelete(parent, curr);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();

        while (curr != null) {
            if (curr.isMarked()) {
                curr = key < curr.key
                        ? curr.left.getReference()
                        : curr.right.getReference();
                continue;
            }

            if (key == curr.key) {
                return true;
            }

            curr = key < curr.key
                    ? curr.left.getReference()
                    : curr.right.getReference();
        }

        return false;
    }

    private SearchResult find(int key) {
        retry:
        while (true) {
            Node parent = null;
            Node curr = root.get();

            while (curr != null) {

                if (curr.isMarked()) {
                    if (!helpDelete(parent, curr)) {
                        continue retry;
                    }

                    curr = parent == null
                            ? root.get()
                            : (key < parent.key
                            ? parent.left.getReference()
                            : parent.right.getReference());

                    continue;
                }

                if (key == curr.key) {
                    return new SearchResult(parent, curr);
                }

                parent = curr;

                curr = key < curr.key
                        ? curr.left.getReference()
                        : curr.right.getReference();
            }

            return new SearchResult(parent, null);
        }
    }

    private boolean helpDelete(Node parent, Node curr) {
        if (curr == null || !curr.isMarked()) {
            return true;
        }

        Node left = curr.left.getReference();
        Node right = curr.right.getReference();

        if (left != null && right != null) {
            return true;
        }

        Node replacement = left != null ? left : right;

        if (parent == null) {
            return root.compareAndSet(curr, replacement) || root.get() != curr;
        }

        if (parent.isMarked()) {
            return false;
        }

        AtomicMarkableReference<Node> childRef =
                curr.key < parent.key ? parent.left : parent.right;

        return childRef.compareAndSet(curr, replacement, false, false)
                || childRef.getReference() != curr;
    }
}