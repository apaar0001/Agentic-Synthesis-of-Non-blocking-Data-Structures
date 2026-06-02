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
            Node current = root.get();
            if (current == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (current.key == key) {
                    return false;
                }
                Node next = (key < current.key) ? current.left.getReference() : current.right.getReference();
                if (next == null) {
                    Node newNode = new Node(key);
                    if ((key < current.key) ? current.left.compareAndSet(null, newNode) : current.right.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    if (next.key == key) {
                        return false;
                    }
                    if (current.left.getReference() == next && current.left.getReference().key == key) {
                        return false;
                    }
                    if (current.right.getReference() == next && current.right.getReference().key == key) {
                        return false;
                    }
                    if (current.left.getReference() == next) {
                        if (next.left.getReference() != null || next.right.getReference() != null) {
                            current = next;
                            continue;
                        }
                    } else if (current.right.getReference() == next) {
                        if (next.left.getReference() != null || next.right.getReference() != null) {
                            current = next;
                            continue;
                        }
                    }
                    if ((key < current.key) ? current.left.compareAndSet(next, newNode) : current.right.compareAndSet(next, newNode)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = root.get();
            if (current == null) {
                return false;
            }
            if (current.key == key) {
                if (current.left.getReference() == null && current.right.getReference() == null) {
                    if (root.compareAndSet(current, null)) {
                        return true;
                    }
                } else if (current.left.getReference() == null) {
                    if (current.right.compareAndSet(current.right.getReference(), null)) {
                        current.right.set(null, false);
                        if (root.compareAndSet(current, current.right.getReference())) {
                            return true;
                        }
                    }
                } else if (current.right.getReference() == null) {
                    if (current.left.compareAndSet(current.left.getReference(), null)) {
                        current.left.set(null, false);
                        if (root.compareAndSet(current, current.left.getReference())) {
                            return true;
                        }
                    }
                } else {
                    Node next = findNext(current);
                    if (next == null) {
                        return false;
                    }
                    if (next.left.getReference() == null && next.right.getReference() == null) {
                        if (next.left.compareAndSet(null, current)) {
                            next.left.set(current, false);
                            if (root.compareAndSet(current, next)) {
                                return true;
                            }
                        }
                    } else if (next.left.getReference() == null) {
                        if (next.right.compareAndSet(next.right.getReference(), current)) {
                            next.right.set(current, false);
                            if (root.compareAndSet(current, next)) {
                                return true;
                            }
                        }
                    } else if (next.right.getReference() == null) {
                        if (next.left.compareAndSet(next.left.getReference(), current)) {
                            next.left.set(current, false);
                            if (root.compareAndSet(current, next)) {
                                return true;
                            }
                        }
                    }
                }
            } else {
                Node next = (key < current.key) ? current.left.getReference() : current.right.getReference();
                if (next == null) {
                    return false;
                }
                if (next.key == key) {
                    if (next.left.getReference() == null && next.right.getReference() == null) {
                        if ((key < current.key) ? current.left.compareAndSet(next, null) : current.right.compareAndSet(next, null)) {
                            return true;
                        }
                    } else if (next.left.getReference() == null) {
                        if (next.right.compareAndSet(next.right.getReference(), null)) {
                            next.right.set(null, false);
                            if ((key < current.key) ? current.left.compareAndSet(next, next.right.getReference()) : current.right.compareAndSet(next, next.right.getReference())) {
                                return true;
                            }
                        }
                    } else if (next.right.getReference() == null) {
                        if (next.left.compareAndSet(next.left.getReference(), null)) {
                            next.left.set(null, false);
                            if ((key < current.key) ? current.left.compareAndSet(next, next.left.getReference()) : current.right.compareAndSet(next, next.left.getReference())) {
                                return true;
                            }
                        }
                    } else {
                        Node nextNext = findNext(next);
                        if (nextNext == null) {
                            return false;
                        }
                        if (nextNext.left.getReference() == null && nextNext.right.getReference() == null) {
                            if (nextNext.left.compareAndSet(null, next)) {
                                nextNext.left.set(next, false);
                                if ((key < current.key) ? current.left.compareAndSet(next, nextNext) : current.right.compareAndSet(next, nextNext)) {
                                    return true;
                                }
                            }
                        } else if (nextNext.left.getReference() == null) {
                            if (nextNext.right.compareAndSet(nextNext.right.getReference(), next)) {
                                nextNext.right.set(next, false);
                                if ((key < current.key) ? current.left.compareAndSet(next, nextNext) : current.right.compareAndSet(next, nextNext)) {
                                    return true;
                                }
                            }
                        } else if (nextNext.right.getReference() == null) {
                            if (nextNext.left.compareAndSet(nextNext.left.getReference(), next)) {
                                nextNext.left.set(next, false);
                                if ((key < current.key) ? current.left.compareAndSet(next, nextNext) : current.right.compareAndSet(next, nextNext)) {
                                    return true;
                                }
                            }
                        }
                    }
                } else {
                    current = next;
                }
            }
        }
    }

    private Node findNext(Node current) {
        Node next = current.right.getReference();
        if (next == null) {
            return null;
        }
        if (next.left.getReference() == null) {
            return next;
        }
        while (true) {
            Node nextNext = next.left.getReference();
            if (nextNext == null) {
                return next;
            }
            next = nextNext;
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node current = root.get();
            if (current == null) {
                return false;
            }
            if (current.key == key) {
                return true;
            }
            Node next = (key < current.key) ? current.left.getReference() : current.right.getReference();
            if (next == null) {
                return false;
            }
            if (next.key == key) {
                return true;
            }
            current = next;
        }
    }
}