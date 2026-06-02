package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                if (root.compareAndSet(null, new Node(key))) {
                    return true;
                }
            } else {
                if (curr.key == key) {
                    return false;
                } else if (curr.key < key) {
                    Node right = curr.right.getReference();
                    if (right == null) {
                        if (curr.right.compareAndSet(null, new Node(key))) {
                            return true;
                        }
                    } else if (right.isMarked()) {
                        helpRemove(curr, right);
                    } else {
                        curr = right;
                    }
                } else {
                    Node left = curr.left.getReference();
                    if (left == null) {
                        if (curr.left.compareAndSet(null, new Node(key))) {
                            return true;
                        }
                    } else if (left.isMarked()) {
                        helpRemove(curr, left);
                    } else {
                        curr = left;
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
            } else if (curr.key == key) {
                if (curr.left.getReference() == null && curr.right.getReference() == null) {
                    if (root.compareAndSet(curr, null)) {
                        return true;
                    }
                } else if (curr.left.getReference() == null) {
                    Node right = curr.right.getReference();
                    if (right.isMarked()) {
                        helpRemove(curr, right);
                    } else if (curr.right.compareAndSet(right, null)) {
                        if (curr.left.compareAndSet(null, null)) {
                            if (root.compareAndSet(curr, right)) {
                                return true;
                            }
                        }
                    }
                } else if (curr.right.getReference() == null) {
                    Node left = curr.left.getReference();
                    if (left.isMarked()) {
                        helpRemove(curr, left);
                    } else if (curr.left.compareAndSet(left, null)) {
                        if (curr.right.compareAndSet(null, null)) {
                            if (root.compareAndSet(curr, left)) {
                                return true;
                            }
                        }
                    }
                } else {
                    AtomicMarkableReference<Node> ref = curr.left.getReference().key < curr.right.getReference().key ? curr.left : curr.right;
                    if (ref.getReference().isMarked()) {
                        helpRemove(curr, ref.getReference());
                    } else if (ref.compareAndSet(ref.getReference(), null)) {
                        if (curr.left.getReference() == null && curr.right.getReference() == null) {
                            if (root.compareAndSet(curr, null)) {
                                return true;
                            }
                        } else if (curr.left.getReference() == null) {
                            if (curr.left.compareAndSet(null, null)) {
                                if (root.compareAndSet(curr, curr.right.getReference())) {
                                    return true;
                                }
                            }
                        } else if (curr.right.getReference() == null) {
                            if (curr.right.compareAndSet(null, null)) {
                                if (root.compareAndSet(curr, curr.left.getReference())) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            } else if (curr.key < key) {
                Node right = curr.right.getReference();
                if (right == null) {
                    return false;
                } else if (right.isMarked()) {
                    helpRemove(curr, right);
                } else {
                    curr = right;
                }
            } else {
                Node left = curr.left.getReference();
                if (left == null) {
                    return false;
                } else if (left.isMarked()) {
                    helpRemove(curr, left);
                } else {
                    curr = left;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                return false;
            } else if (curr.key == key) {
                return !curr.left.getReference().isMarked() && !curr.right.getReference().isMarked();
            } else if (curr.key < key) {
                Node right = curr.right.getReference();
                if (right == null) {
                    return false;
                } else if (right.isMarked()) {
                    helpRemove(curr, right);
                } else {
                    curr = right;
                }
            } else {
                Node left = curr.left.getReference();
                if (left == null) {
                    return false;
                } else if (left.isMarked()) {
                    helpRemove(curr, left);
                } else {
                    curr = left;
                }
            }
        }
    }

    private void helpRemove(Node parent, Node node) {
        if (node.isMarked()) {
            if (parent.left.getReference() == node) {
                parent.left.compareAndSet(node, null);
            } else if (parent.right.getReference() == node) {
                parent.right.compareAndSet(node, null);
            }
            // Node has been marked
        }
    }
}