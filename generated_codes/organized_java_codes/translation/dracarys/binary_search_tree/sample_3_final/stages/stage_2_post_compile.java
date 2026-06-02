package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

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

    private AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicMarkableReference<>(null, false);
    }

    private Node getPredecessor(Node node, int key) {
        Node predecessor = null;
        Node current = node;
        while (current != null) {
            if (current.key == key) {
                return predecessor;
            }
            predecessor = current;
            if (key < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }
        return predecessor;
    }

    private Node getSuccessor(Node node, int key) {
        Node successor = null;
        Node current = node;
        while (current != null) {
            if (current.key == key) {
                return successor;
            }
            successor = current;
            if (key < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }
        return successor;
    }

    private Node findNode(Node node, int key) {
        while (node != null) {
            if (node.key == key) {
                return node;
            }
            if (key < node.key) {
                node = node.left.getReference();
            } else {
                node = node.right.getReference();
            }
        }
        return null;
    }

    private boolean addNode(Node node, int key) {
        Node newNode = new Node(key);
        while (true) {
            if (node == null) {
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                return false;
            }
            if (key < node.key) {
                if (node.left.getReference() == null) {
                    if (node.left.compareAndSet(null, newNode, false, false)) {
                        return true;
                    }
                } else {
                    node = node.left.getReference();
                }
            } else {
                if (node.right.getReference() == null) {
                    if (node.right.compareAndSet(null, newNode, false, false)) {
                        return true;
                    }
                } else {
                    node = node.right.getReference();
                }
            }
        }
    }

    private boolean removeNode(Node node, int key) {
        Node predecessor = getPredecessor(node, key);
        if (predecessor == null) {
            return false;
        }
        Node successor = getSuccessor(node, key);
        if (successor == null) {
            return false;
        }
        if (predecessor.key < key && key < successor.key) {
            if (node.left.getReference() == predecessor && node.right.getReference() == successor) {
                if (node.left.compareAndSet(predecessor, successor, false, false) && node.right.compareAndSet(successor, predecessor, false, false)) {
                    // Node has been marked
                    return true;
                }
            }
        }
        return false;
    }

    private boolean containsNode(Node node, int key) {
        while (node != null) {
            if (node.key == key) {
                return true;
            }
            if (key < node.key) {
                node = node.left.getReference();
            } else {
                node = node.right.getReference();
            }
        }
        return false;
    }

    @Override
    public boolean add(int key) {
        Node node = root.getReference();
        if (node == null) {
            return addNode(node, key);
        }
        while (true) {
            if (node.key == key) {
                return false;
            }
            if (key < node.key) {
                if (node.left.getReference() == null) {
                    return addNode(node, key);
                }
                node = node.left.getReference();
            } else {
                if (node.right.getReference() == null) {
                    return addNode(node, key);
                }
                node = node.right.getReference();
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node node = root.getReference();
        if (node == null) {
            return false;
        }
        while (true) {
            if (node.key == key) {
                return removeNode(node, key);
            }
            if (key < node.key) {
                node = node.left.getReference();
            } else {
                node = node.right.getReference();
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node node = root.getReference();
        if (node == null) {
            return false;
        }
        while (true) {
            if (node.key == key) {
                return true;
            }
            if (key < node.key) {
                node = node.left.getReference();
            } else {
                node = node.right.getReference();
            }
        }
    }
}