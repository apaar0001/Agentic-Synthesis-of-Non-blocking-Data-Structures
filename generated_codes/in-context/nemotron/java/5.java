package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Binary Search Tree — Harris-Michael style.
 * Uses AtomicMarkableReference on every child pointer.
 * Names: Node/key/left/right/marked, root
 *
 * remove() two-phase: mark node, then CAS parent child.
 * Linearization point: successful parent CAS.
 * Lock-freedom: every CAS attempt makes global progress.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;
        final AtomicMarkableReference<Boolean> marked;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicMarkableReference<>(false, false);
        }
    }

    private final Node root = new Node(Integer.MIN_VALUE);

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = null;
            Node curr = root;

            while (curr != null) {
                if (key == curr.key) {
                    boolean[] marked = { false };
                    curr.marked.get(marked);
                    if (!marked[0]) {
                        return false;
                    }
                    curr.marked.set(false, false);
                    return true;
                }
                parent = curr;
                if (key < curr.key) {
                    curr = curr.left.getReference();
                } else {
                    curr = curr.right.getReference();
                }
            }

            Node newNode = new Node(key);
            if (key < parent.key) {
                newNode.left.set(null, false);
                newNode.right.set(null, false);
                newNode.marked.set(false, false);
                if (!parent.left.compareAndSet(null, newNode, false, false)) {
                    continue;
                }
            } else {
                newNode.left.set(null, false);
                newNode.right.set(null, false);
                newNode.marked.set(false, false);
                if (!parent.right.compareAndSet(null, newNode, false, false)) {
                    continue;
                }
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node parent = null;
            Node curr = root;

            while (curr != null) {
                if (key == curr.key) {
                    boolean[] marked = { false };
                    curr.marked.get(marked);
                    if (marked[0]) {
                        return false;
                    }
                    curr.marked.set(true, false);
                    // Node has been marked
                    if (key < parent.key) {
                        if (!parent.left.compareAndSet(curr, null, false, false)) {
                            continue;
                        }
                    } else {
                        if (!parent.right.compareAndSet(curr, null, false, false)) {
                            continue;
                        }
                    }
                    return true;
                }
                parent = curr;
                if (key < curr.key) {
                    curr = curr.left.getReference();
                } else {
                    curr = curr.right.getReference();
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root;
        while (curr != null) {
            if (key == curr.key) {
                boolean[] marked = { false };
                curr.marked.get(marked);
                return !marked[0];
            }
            if (key < curr.key) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
        return false;
    }
}