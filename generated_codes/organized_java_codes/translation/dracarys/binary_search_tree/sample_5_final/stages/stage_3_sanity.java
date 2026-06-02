package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left;
        final AtomicReference<Node> right;
        final AtomicMarkableReference<Node> marked;

        Node(int key, Node left, Node right) {
            this.key = key;
            this.left = new AtomicReference<>(left);
            this.right = new AtomicReference<>(right);
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                if (root.compareAndSet(null, new Node(key, null, null))) {
                    return true;
                }
            } else {
                Node next = findNext(curr, key);
                if (next == null) {
                    return false;
                }
                Node newNode = new Node(key, null, null);
                if (key < next.key) {
                    if (next.left.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    if (next.right.compareAndSet(null, newNode)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                return false;
            }
            Node next = findNext(curr, key);
            if (next == null || next.key != key) {
                return false;
            }
            if (next.marked.getReference() != null) {
                return false;
            }
            if (next.left.get() == null && next.right.get() == null) {
                if (next.marked.compareAndSet(null, next, false, true)) {
                    // Node has been marked
                    Node parent = findParent(curr, key);
                    if (key < parent.key) {
                        parent.left.compareAndSet(next, null);
                    } else {
                        parent.right.compareAndSet(next, null);
                    }
                    return true;
                }
            } else if (next.left.get() == null) {
                if (next.marked.compareAndSet(null, next, false, true)) {
                    // Node has been marked
                    Node parent = findParent(curr, key);
                    if (key < parent.key) {
                        parent.left.compareAndSet(next, next.right.get());
                    } else {
                        parent.right.compareAndSet(next, next.right.get());
                    }
                    return true;
                }
            } else if (next.right.get() == null) {
                if (next.marked.compareAndSet(null, next, false, true)) {
                    // Node has been marked
                    Node parent = findParent(curr, key);
                    if (key < parent.key) {
                        parent.left.compareAndSet(next, next.left.get());
                    } else {
                        parent.right.compareAndSet(next, next.left.get());
                    }
                    return true;
                }
            } else {
                Node replacement = findMin(next.right.get());
                if (replacement.marked.getReference() != null) {
                    continue;
                }
                if (next.marked.compareAndSet(null, next, false, true)) {
                    // Node has been marked
                    Node parent = findParent(curr, key);
                    if (key < parent.key) {
                        parent.left.compareAndSet(next, replacement);
                    } else {
                        parent.right.compareAndSet(next, replacement);
                    }
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();
        while (curr != null) {
            if (curr.key == key && curr.marked.getReference() == null) {
                return true;
            }
            if (key < curr.key) {
                curr = curr.left.get();
            } else {
                curr = curr.right.get();
            }
        }
        return false;
    }

    private Node findNext(Node curr, int key) {
        if (curr == null) {
            return null;
        }
        if (key < curr.key) {
            Node left = curr.left.get();
            if (left == null || left.marked.getReference() == null) {
                return curr;
            }
            return findNext(left, key);
        } else {
            Node right = curr.right.get();
            if (right == null || right.marked.getReference() == null) {
                return curr;
            }
            return findNext(right, key);
        }
    }

    private Node findParent(Node curr, int key) {
        if (curr == null) {
            return null;
        }
        if (key < curr.key) {
            Node left = curr.left.get();
            if (left == null || left.key != key) {
                return curr;
            }
            return findParent(left, key);
        } else {
            Node right = curr.right.get();
            if (right == null || right.key != key) {
                return curr;
            }
            return findParent(right, key);
        }
    }

    private Node findMin(Node curr) {
        while (curr.left.get() != null) {
            curr = curr.left.get();
        }
        return curr;
    }
}