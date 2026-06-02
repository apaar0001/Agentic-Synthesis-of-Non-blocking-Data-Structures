package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
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
            AtomicMarkableReference<Node> currRef = root;
            Node parent = null;
            boolean restart = false;
            while (true) {
                Node curr = currRef.getReference();
                boolean marked = currRef.isMarked();
                if (marked) {
                    helpRemove(parent, currRef, curr);
                    restart = true;
                    break;
                }
                if (curr == null) {
                    if (currRef.compareAndSet(null, newNode, false, false)) {
                        return true;
                    } else {
                        restart = true;
                        break;
                    }
                }
                if (key == curr.key) {
                    return false;
                }
                boolean goLeft = key < curr.key;
                parent = curr;
                currRef = goLeft ? curr.left : curr.right;
            }
            if (!restart) {
                // Should not reach here
                break;
            }
            // restart outer while loop
        }
    }

    @Override
    public boolean contains(int key) {
        AtomicMarkableReference<Node> currRef = root;
        Node parent = null;
        while (true) {
            Node curr = currRef.getReference();
            boolean marked = currRef.isMarked();
            if (marked) {
                helpRemove(parent, currRef, curr);
                parent = null;
                currRef = root;
                continue;
            }
            if (curr == null) {
                return false;
            }
            if (key == curr.key) {
                return true;
            }
            boolean goLeft = key < curr.key;
            parent = curr;
            currRef = goLeft ? curr.left : curr.right;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            AtomicMarkableReference<Node> currRef = root;
            Node parent = null;
            boolean restart = false;
            while (true) {
                Node curr = currRef.getReference();
                boolean marked = currRef.isMarked();
                if (marked) {
                    helpRemove(parent, currRef, curr);
                    restart = true;
                    break;
                }
                if (curr == null) {
                    return false;
                }
                if (key == curr.key) {
                    if (currRef.attemptMark(curr, true)) {
                        // Node has been marked
                        helpRemove(parent, currRef, curr);
                        return true;
                    } else {
                        restart = true;
                        break;
                    }
                }
                boolean goLeft = key < curr.key;
                parent = curr;
                currRef = goLeft ? curr.left : curr.right;
            }
            if (!restart) {
                break;
            }
        }
    }

    private void helpRemove(Node parent, AtomicMarkableReference<Node> link, Node victim) {
        // Assume victim is marked
        Node left = victim.left.getReference();
        boolean leftMarked = victim.left.isMarked();
        Node right = victim.right.getReference();
        boolean rightMarked = victim.right.isMarked();

        // Help remove marked children
        while (left != null && leftMarked) {
            helpRemove(victim, victim.left, left);
            left = victim.left.getReference();
            leftMarked = victim.left.isMarked();
        }
        while (right != null && rightMarked) {
            helpRemove(victim, victim.right, right);
            right = victim.right.getReference();
            rightMarked = victim.right.isMarked();
        }

        Node replacement;
        if (left == null && right == null) {
            replacement = null;
        } else if (left == null) {
            replacement = right;
        } else if (right == null) {
            replacement = left;
        } else {
            // Two children: cannot remove now
            return;
        }

        // Attempt to replace victim with replacement (or null) and unmark
        link.compareAndSet(victim, replacement, true, false);
    }
}