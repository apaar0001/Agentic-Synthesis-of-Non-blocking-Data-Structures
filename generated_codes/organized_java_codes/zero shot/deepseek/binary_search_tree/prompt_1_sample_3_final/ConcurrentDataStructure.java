package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Unit {
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
    }

    private final AtomicMarkableReference<Unit> root;

    public ConcurrentDataStructure() {
        Unit initial = new Unit(null, null, null, true);
        root = new AtomicMarkableReference<>(initial, false);
    }

    @Override
    public boolean contains(int k) {
        Unit current = root.getReference();

        while (!current.isLeaf) {
            if (k < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }

        return current.key != null && current.key == k && !current.left.isMarked();
    }

    @Override
    public boolean add(int k) {
        while (true) {
            Unit parent = null;
            Unit current = root.getReference();

            while (!current.isLeaf) {
                parent = current;
                if (k < current.key) {
                    current = current.left.getReference();
                } else {
                    current = current.right.getReference();
                }
            }

            if (current.key != null && current.key == k) {
                if (current.left.isMarked()) {
                    continue;
                }
                return false;
            }

            if (current.key == null) {
                Unit newLeaf = new Unit(k, null, null, true);
                if (root.compareAndSet(current, newLeaf, false, false)) {
                    return true;
                }
                continue;
            }

            int oldKey = current.key;
            Unit newLeaf = new Unit(k, null, null, true);
            Unit oldLeaf = new Unit(oldKey, null, null, true);
            Unit newInternal = new Unit(null, null, null, false);

            if (k < oldKey) {
                newInternal.key = oldKey;
                newInternal.left.set(newLeaf, false);
                newInternal.right.set(oldLeaf, false);
            } else {
                newInternal.key = k;
                newInternal.left.set(oldLeaf, false);
                newInternal.right.set(newLeaf, false);
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
    public boolean remove(int k) {
        while (true) {
            Unit parent = null;
            Unit grandParent = null;
            Unit current = root.getReference();

            while (!current.isLeaf) {
                grandParent = parent;
                parent = current;
                if (k < current.key) {
                    current = current.left.getReference();
                } else {
                    current = current.right.getReference();
                }
            }

            if (current.key == null || current.key != k) {
                return false;
            }

            if (current.left.isMarked()) {
                return false;
            }

            if (parent == null) {
                if (current.left.compareAndSet(null, null, false, true)) {
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

            if (current.left.compareAndSet(null, null, false, true)) {
                // Node has been marked
                if (grandParent == null) {
                    if (root.compareAndSet(parent, sibling, false, false)) {
                        return true;
                    }
                } else {
                    if (grandParent.left.getReference() == parent) {
                        if (grandParent.left.compareAndSet(parent, sibling, false, false)) {
                            return true;
                        }
                    } else {
                        if (grandParent.right.compareAndSet(parent, sibling, false, false)) {
                            return true;
                        }
                    }
                }
            }
        }
    }
}