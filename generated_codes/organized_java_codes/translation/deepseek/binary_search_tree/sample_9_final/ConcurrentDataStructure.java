package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        final int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        Node newNode = new Node(key);
        while (true) {
            Node current = root.get();
            if (current == null) {
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
                continue;
            }

            Node parent = null;
            Node child = current;
            AtomicMarkableReference<Node> childRef = null;

            while (child != null) {
                parent = child;
                if (key == child.key) {
                    boolean[] marked = {false};
                    Node next = child.left.get(marked);
                    if (marked[0]) {
                        helpRemove(parent, child);
                        return add(key);
                    }
                    return false;
                } else if (key < child.key) {
                    childRef = child.left;
                } else {
                    childRef = child.right;
                }

                boolean[] marked = {false};
                Node next = childRef.get(marked);
                if (marked[0]) {
                    helpRemove(parent, child);
                    return add(key);
                }
                child = next;
            }

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
            Node parent = null;
            Node current = root.get();
            AtomicMarkableReference<Node> childRef = root;

            while (current != null) {
                boolean[] marked = {false};
                Node left = current.left.get(marked);
                if (marked[0]) {
                    helpRemove(parent, current);
                    return remove(key);
                }

                if (key == current.key) {
                    if (current.left.compareAndSet(left, left, false, true)) {
                        // Node has been marked
                        helpRemove(parent, current);
                        return true;
                    }
                    continue;
                }

                parent = current;
                if (key < current.key) {
                    childRef = current.left;
                    current = left;
                } else {
                    childRef = current.right;
                    current = current.right.getReference();
                }
            }
            return false;
        }
    }

    private void helpRemove(Node parent, Node target) {
        Node left = target.left.getReference();
        Node right = target.right.getReference();

        if (left == null && right == null) {
            if (parent == null) {
                root.compareAndSet(target, null);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, null, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, null, false, false);
            }
        } else if (left != null && right == null) {
            if (parent == null) {
                root.compareAndSet(target, left);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, left, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, left, false, false);
            }
        } else if (left == null && right != null) {
            if (parent == null) {
                root.compareAndSet(target, right);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, right, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, right, false, false);
            }
        } else {
            Node successor = right;
            Node successorParent = target;
            while (successor.left.getReference() != null) {
                successorParent = successor;
                successor = successor.left.getReference();
            }

            if (successorParent != target) {
                successorParent.left.set(successor.right.getReference(), false);
                successor.right.set(right, false);
            }
            successor.left.set(left, false);

            if (parent == null) {
                root.compareAndSet(target, successor);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, successor, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, successor, false, false);
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node current = root.get();
        while (current != null) {
            boolean[] marked = {false};
            Node left = current.left.get(marked);
            if (marked[0]) {
                current = root.get();
                continue;
            }

            if (key == current.key) {
                return true;
            } else if (key < current.key) {
                current = left;
            } else {
                current = current.right.getReference();
            }
        }
        return false;
    }
}