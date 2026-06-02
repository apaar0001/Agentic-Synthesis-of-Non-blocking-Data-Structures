package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final class Unit {
        final Integer key;
        final AtomicMarkableReference<Unit> left;
        final AtomicMarkableReference<Unit> right;
        final boolean isLeaf;

        Unit(Integer key, Unit left, Unit right, boolean isLeaf) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(left, false);
            this.right = new AtomicMarkableReference<>(right, false);
            this.isLeaf = isLeaf;
        }

        Unit(Integer key, boolean isLeaf) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.isLeaf = isLeaf;
        }
    }

    private final AtomicMarkableReference<Unit> root;

    public ConcurrentDataStructure() {
        Unit initial = new Unit(null, true);
        root = new AtomicMarkableReference<>(initial, false);
    }

    @Override
    public boolean contains(int key) {
        Unit current = root.getReference();

        while (!current.isLeaf) {
            if (key < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }

        return current.key != null && current.key == key;
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Unit parent = null;
            Unit current = root.getReference();

            while (!current.isLeaf) {
                parent = current;
                if (key < current.key) {
                    current = current.left.getReference();
                } else {
                    current = current.right.getReference();
                }
            }

            if (current.key != null && current.key == key) {
                return false;
            }

            if (current.key == null) {
                Unit newLeaf = new Unit(key, true);
                if (parent == null) {
                    if (root.compareAndSet(current, newLeaf, false, false)) {
                        return true;
                    }
                } else {
                    if (parent.left.getReference() == current) {
                        if (parent.left.compareAndSet(current, newLeaf, false, false)) {
                            return true;
                        }
                    } else {
                        if (parent.right.compareAndSet(current, newLeaf, false, false)) {
                            return true;
                        }
                    }
                }
                continue;
            }

            int oldKey = current.key;
            Unit newLeaf = new Unit(key, true);
            Unit oldLeaf = new Unit(oldKey, true);
            Unit newInternal = new Unit(null, null, null, false);

            if (key < oldKey) {
                newInternal.left.set(newLeaf, false);
                newInternal.right.set(oldLeaf, false);
                newInternal.key = oldKey;
            } else {
                newInternal.left.set(oldLeaf, false);
                newInternal.right.set(newLeaf, false);
                newInternal.key = key;
            }

            if (parent == null) {
                if (root.compareAndSet(current, newInternal, false, false)) {
                    return true;
                }
            } else {
                if (parent.left.getReference() == current) {
                    if (parent.left.compareAndSet(current, newInternal, false, false)) {
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(current, newInternal, false, false)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Unit grandParent = null;
            Unit parent = null;
            Unit current = root.getReference();

            while (!current.isLeaf) {
                grandParent = parent;
                parent = current;
                if (key < current.key) {
                    current = current.left.getReference();
                } else {
                    current = current.right.getReference();
                }
            }

            if (current.key == null || current.key != key) {
                return false;
            }

            if (parent == null) {
                Unit newLeaf = new Unit(null, true);
                if (root.compareAndSet(current, newLeaf, false, false)) {
                    // Node has been marked
                    return true;
                }
                continue;
            }

            Unit sibling;
            if (parent.left.getReference() == current) {
                sibling = parent.right.getReference();
            } else {
                sibling = parent.left.getReference();
            }

            if (grandParent == null) {
                if (root.compareAndSet(parent, sibling, false, false)) {
                    // Node has been marked
                    return true;
                }
                continue;
            }

            if (grandParent.left.getReference() == parent) {
                if (grandParent.left.compareAndSet(parent, sibling, false, false)) {
                    // Node has been marked
                    return true;
                }
            } else {
                if (grandParent.right.compareAndSet(parent, sibling, false, false)) {
                    // Node has been marked
                    return true;
                }
            }
        }
    }
}