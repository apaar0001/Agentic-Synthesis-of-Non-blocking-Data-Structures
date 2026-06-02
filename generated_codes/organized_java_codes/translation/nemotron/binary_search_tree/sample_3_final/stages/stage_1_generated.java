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

    private final AtomicReference<Node> root = new AtomicReference<>(null);

    public ConcurrentDataStructure() {
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node newNode = new Node(key);
            Node curr = root.get();
            if (curr == null) {
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
                continue;
            }
            Node parent = null;
            AtomicMarkableReference<Node> parentRef = null;
            boolean isLeft = false;
            while (true) {
                if (parentRef != null && parentRef.isMarked()) {
                    helpRemoveNode(parentRef, isLeft, curr);
                    curr = root.get();
                    parent = null;
                    parentRef = null;
                    isLeft = false;
                    break;
                }
                if (key < curr.key) {
                    parent = curr;
                    parentRef = curr.left;
                    isLeft = true;
                    Node next = curr.left.getReference();
                    if (next == null) {
                        if (curr.left.compareAndSet(null, newNode, false, false)) {
                            return true;
                        }
                        break;
                    }
                    curr = next;
                } else if (key > curr.key) {
                    parent = curr;
                    parentRef = curr.right;
                    isLeft = false;
                    Node next = curr.right.getReference();
                    if (next == null) {
                        if (curr.right.compareAndSet(null, newNode, false, false)) {
                            return true;
                        }
                        break;
                    }
                    curr = next;
                } else {
                    return false;
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
            Node parent = null;
            AtomicMarkableReference<Node> parentRef = null;
            boolean isLeft = false;
            while (true) {
                if (parentRef != null && parentRef.isMarked()) {
                    helpRemoveNode(parentRef, isLeft, curr);
                    curr = root.get();
                    parent = null;
                    parentRef = null;
                    isLeft = false;
                    break;
                }
                if (key < curr.key) {
                    parent = curr;
                    parentRef = curr.left;
                    isLeft = true;
                    curr = curr.left.getReference();
                } else if (key > curr.key) {
                    parent = curr;
                    parentRef = curr.right;
                    isLeft = false;
                    curr = curr.right.getReference();
                } else {
                    AtomicMarkableReference<Node> refToUse = (parent == null) ? new AtomicMarkableReference<>(root.get(), false) : parentRef;
                    boolean marked;
                    if (parent == null) {
                        marked = root.compareAndSet(curr, curr, false, true);
                    } else {
                        marked = parentRef.compareAndSet(curr, curr, false, true);
                    }
                    if (!marked) {
                        curr = root.get();
                        parent = null;
                        parentRef = null;
                        isLeft = false;
                        break;
                    }
                    // Node has been marked
                    Node replacement = (curr.left.getReference() != null) ? curr.left.getReference() : curr.right.getReference();
                    boolean unlinked;
                    if (parent == null) {
                        unlinked = root.compareAndSet(curr, replacement, true, false);
                    } else {
                        unlinked = parentRef.compareAndSet(curr, replacement, true, false);
                    }
                    if (!unlinked) {
                        helpRemoveNode(parentRef, isLeft, curr);
                        curr = root.get();
                        parent = null;
                        parentRef = null;
                        isLeft = false;
                        break;
                    }
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();
        AtomicMarkableReference<Node> parentRef = null;
        Node parent = null;
        boolean isLeft = false;
        while (true) {
            if (curr == null) {
                return false;
            }
            if (parentRef != null && parentRef.isMarked()) {
                helpRemoveNode(parentRef, isLeft, curr);
                curr = root.get();
                parent = null;
                parentRef = null;
                isLeft = false;
                continue;
            }
            if (key < curr.key) {
                parent = curr;
                parentRef = curr.left;
                isLeft = true;
                curr = curr.left.getReference();
            } else if (key > curr.key) {
                parent = curr;
                parentRef = curr.right;
                isLeft = false;
                curr = curr.right.getReference();
            } else {
                return true;
            }
        }
    }

    private void helpRemoveNode(AtomicMarkableReference<Node> parentRef, boolean isLeft, Node victim) {
        Node replacement = (victim.left.getReference() != null) ? victim.left.getReference() : victim.right.getReference();
        if (parentRef != null) {
            parentRef.compareAndSet(victim, replacement, true, false);
        }
    }
}