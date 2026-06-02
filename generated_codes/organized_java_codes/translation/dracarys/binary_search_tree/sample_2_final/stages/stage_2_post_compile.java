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

        Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>(null);
            this.right = new AtomicReference<>(null);
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (curr.marked.getReference() != null) {
                    helpRemove(curr);
                    continue;
                }
                if (key < curr.key) {
                    Node leftChild = curr.left.get();
                    if (leftChild == null) {
                        Node newNode = new Node(key);
                        if (curr.left.compareAndSet(null, newNode)) {
                            return true;
                        }
                    } else {
                        if (leftChild.marked.getReference() != null) {
                            helpRemove(leftChild);
                            continue;
                        }
                        curr = leftChild;
                    }
                } else if (key > curr.key) {
                    Node rightChild = curr.right.get();
                    if (rightChild == null) {
                        Node newNode = new Node(key);
                        if (curr.right.compareAndSet(null, newNode)) {
                            return true;
                        }
                    } else {
                        if (rightChild.marked.getReference() != null) {
                            helpRemove(rightChild);
                            continue;
                        }
                        curr = rightChild;
                    }
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
            } else {
                if (curr.marked.getReference() != null) {
                    helpRemove(curr);
                    continue;
                }
                if (key < curr.key) {
                    Node leftChild = curr.left.get();
                    if (leftChild == null) {
                        return false;
                    } else {
                        if (leftChild.marked.getReference() != null) {
                            helpRemove(leftChild);
                            continue;
                        }
                        curr = leftChild;
                    }
                } else if (key > curr.key) {
                    Node rightChild = curr.right.get();
                    if (rightChild == null) {
                        return false;
                    } else {
                        if (rightChild.marked.getReference() != null) {
                            helpRemove(rightChild);
                            continue;
                        }
                        curr = rightChild;
                    }
                } else {
                    if (curr.marked.attemptMark(null, true)) {
                        // Node has been marked
                        helpRemove(curr);
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();
        while (curr != null) {
            if (curr.marked.getReference() != null) {
                helpRemove(curr);
                continue;
            }
            if (key < curr.key) {
                curr = curr.left.get();
            } else if (key > curr.key) {
                curr = curr.right.get();
            } else {
                return true;
            }
        }
        return false;
    }

    private void helpRemove(Node node) {
        while (true) {
            Node leftChild = node.left.get();
            Node rightChild = node.right.get();
            if (leftChild == null && rightChild == null) {
                if (node.marked.getReference() == null) {
                    return;
                }
                Node parent = findParent(node);
                if (parent == null) {
                    if (root.compareAndSet(node, null)) {
                        return;
                    }
                } else {
                    if (parent.left.get() == node) {
                        if (parent.left.compareAndSet(node, null)) {
                            return;
                        }
                    } else {
                        if (parent.right.compareAndSet(node, null)) {
                            return;
                        }
                    }
                }
            } else if (leftChild == null) {
                Node parent = findParent(node);
                if (parent == null) {
                    if (root.compareAndSet(node, rightChild)) {
                        return;
                    }
                } else {
                    if (parent.left.get() == node) {
                        if (parent.left.compareAndSet(node, rightChild)) {
                            return;
                        }
                    } else {
                        if (parent.right.compareAndSet(node, rightChild)) {
                            return;
                        }
                    }
                }
            } else if (rightChild == null) {
                Node parent = findParent(node);
                if (parent == null) {
                    if (root.compareAndSet(node, leftChild)) {
                        return;
                    }
                } else {
                    if (parent.left.get() == node) {
                        if (parent.left.compareAndSet(node, leftChild)) {
                            return;
                        }
                    } else {
                        if (parent.right.compareAndSet(node, leftChild)) {
                            return;
                        }
                    }
                }
            } else {
                Node successor = findSuccessor(node);
                if (successor.marked.getReference() != null) {
                    helpRemove(successor);
                    continue;
                }
                Node parent = findParent(node);
                if (parent == null) {
                    if (root.compareAndSet(node, successor)) {
                        return;
                    }
                } else {
                    if (parent.left.get() == node) {
                        if (parent.left.compareAndSet(node, successor)) {
                            return;
                        }
                    } else {
                        if (parent.right.compareAndSet(node, successor)) {
                            return;
                        }
                    }
                }
            }
        }
    }

    private Node findParent(Node node) {
        Node curr = root.get();
        while (curr != null) {
            if (curr.left.get() == node || curr.right.get() == node) {
                return curr;
            }
            if (node.key < curr.key) {
                curr = curr.left.get();
            } else {
                curr = curr.right.get();
            }
        }
        return null;
    }

    private Node findSuccessor(Node node) {
        Node curr = node.right.get();
        while (curr.left.get() != null) {
            curr = curr.left.get();
        }
        return curr;
    }
}