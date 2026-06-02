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

    private AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicMarkableReference<>(null, false);
    }

    private boolean add(Node node, int key, Node parent) {
        if (key < node.key) {
            if (node.left.getReference() == null) {
                Node newNode = new Node(key);
                if (node.left.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            } else if (!node.left.getReference().marked) {
                return add(node.left.getReference(), key, node);
            } else {
                Node newLeft = findNext(node.left.getReference());
                if (newLeft == null) {
                    return false;
                }
                if (newLeft.key < node.key) {
                    return add(newLeft, key, node);
                } else {
                    return false;
                }
            }
        } else if (key > node.key) {
            if (node.right.getReference() == null) {
                Node newNode = new Node(key);
                if (node.right.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            } else if (!node.right.getReference().marked) {
                return add(node.right.getReference(), key, node);
            } else {
                Node newRight = findNext(node.right.getReference());
                if (newRight == null) {
                    return false;
                }
                if (newRight.key > node.key) {
                    return add(newRight, key, node);
                } else {
                    return false;
                }
            }
        } else {
            return false;
        }
    }

    private boolean remove(Node node, int key, Node parent) {
        if (key < node.key) {
            if (node.left.getReference() == null) {
                return false;
            } else if (!node.left.getReference().marked) {
                return remove(node.left.getReference(), key, node);
            } else {
                Node newLeft = findNext(node.left.getReference());
                if (newLeft == null) {
                    return false;
                }
                if (newLeft.key < node.key) {
                    return remove(newLeft, key, node);
                } else {
                    return false;
                }
            }
        } else if (key > node.key) {
            if (node.right.getReference() == null) {
                return false;
            } else if (!node.right.getReference().marked) {
                return remove(node.right.getReference(), key, node);
            } else {
                Node newRight = findNext(node.right.getReference());
                if (newRight == null) {
                    return false;
                }
                if (newRight.key > node.key) {
                    return remove(newRight, key, node);
                } else {
                    return false;
                }
            }
        } else {
            if (node.left.getReference() == null && node.right.getReference() == null) {
                if (node.marked) {
                    return false;
                }
                if (node.left.compareAndSet(null, null, false, true) && node.right.compareAndSet(null, null, false, true)) {
                    if (parent.left.getReference() == node) {
                        parent.left.compareAndSet(node, null, false, false);
                    } else {
                        parent.right.compareAndSet(node, null, false, false);
                    }
                    // Node has been marked
                    return true;
                }
            } else if (node.left.getReference() == null) {
                if (node.right.getReference().marked) {
                    return false;
                }
                Node newRight = findNext(node.right.getReference());
                if (newRight == null) {
                    return false;
                }
                if (newRight.key > node.key) {
                    return remove(newRight, key, node);
                } else {
                    return false;
                }
            } else if (node.right.getReference() == null) {
                if (node.left.getReference().marked) {
                    return false;
                }
                Node newLeft = findNext(node.left.getReference());
                if (newLeft == null) {
                    return false;
                }
                if (newLeft.key < node.key) {
                    return remove(newLeft, key, node);
                } else {
                    return false;
                }
            } else {
                Node successor = findSuccessor(node);
                if (successor == null) {
                    return false;
                }
                if (successor.key > node.key) {
                    return remove(successor, key, node);
                } else {
                    return false;
                }
            }
        }
    }

    private boolean contains(Node node, int key) {
        if (node == null) {
            return false;
        }
        if (key < node.key) {
            if (node.left.getReference() == null) {
                return false;
            } else if (!node.left.getReference().marked) {
                return contains(node.left.getReference(), key);
            } else {
                Node newLeft = findNext(node.left.getReference());
                if (newLeft == null) {
                    return false;
                }
                if (newLeft.key < node.key) {
                    return contains(newLeft, key);
                } else {
                    return false;
                }
            }
        } else if (key > node.key) {
            if (node.right.getReference() == null) {
                return false;
            } else if (!node.right.getReference().marked) {
                return contains(node.right.getReference(), key);
            } else {
                Node newRight = findNext(node.right.getReference());
                if (newRight == null) {
                    return false;
                }
                if (newRight.key > node.key) {
                    return contains(newRight, key);
                } else {
                    return false;
                }
            }
        } else {
            return !node.marked;
        }
    }

    private Node findNext(Node node) {
        if (node.left.getReference() != null && !node.left.getReference().marked) {
            return node.left.getReference();
        } else if (node.right.getReference() != null && !node.right.getReference().marked) {
            return node.right.getReference();
        } else {
            return null;
        }
    }

    private Node findSuccessor(Node node) {
        Node successor = node.right.getReference();
        while (successor != null && !successor.marked) {
            if (successor.left.getReference() != null && !successor.left.getReference().marked) {
                successor = successor.left.getReference();
            } else {
                break;
            }
        }
        return successor;
    }

    @Override
    public boolean add(int key) {
        Node node = root.getReference();
        if (node == null) {
            Node newNode = new Node(key);
            if (root.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        } else if (!node.marked) {
            return add(node, key, null);
        } else {
            Node newRoot = findNext(node);
            if (newRoot == null) {
                return false;
            }
            if (newRoot.key < node.key) {
                return add(newRoot, key, null);
            } else {
                return false;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node node = root.getReference();
        if (node == null) {
            return false;
        } else if (!node.marked) {
            return remove(node, key, null);
        } else {
            Node newRoot = findNext(node);
            if (newRoot == null) {
                return false;
            }
            if (newRoot.key < node.key) {
                return remove(newRoot, key, null);
            } else {
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node node = root.getReference();
        if (node == null) {
            return false;
        } else if (!node.marked) {
            return contains(node, key);
        } else {
            Node newRoot = findNext(node);
            if (newRoot == null) {
                return false;
            }
            if (newRoot.key < node.key) {
                return contains(newRoot, key);
            } else {
                return false;
            }
        }
    }
}