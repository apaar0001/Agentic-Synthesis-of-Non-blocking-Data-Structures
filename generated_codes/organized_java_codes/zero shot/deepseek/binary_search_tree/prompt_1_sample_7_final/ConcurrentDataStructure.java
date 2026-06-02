package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final class Unit {
        final Integer key;
        final AtomicMarkableReference<Unit> left;
        final AtomicMarkableReference<Unit> right;
        final boolean isLeaf;
        final AtomicMarkableReference<Boolean> marked;

        Unit(Integer key, boolean isLeaf) {
            this.key = key;
            this.isLeaf = isLeaf;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicMarkableReference<>(false, false);
        }
    }

    private final AtomicReference<Unit> root;

    public ConcurrentDataStructure() {
        Unit initial = new Unit(null, true);
        root = new AtomicReference<>(initial);
    }

    private Unit find(int key, Unit[] parent, Unit[] grandParent) {
        while (true) {
            Unit current = root.get();
            parent[0] = null;
            grandParent[0] = null;
            
            while (!current.isLeaf) {
                grandParent[0] = parent[0];
                parent[0] = current;
                
                boolean[] mark = new boolean[1];
                Unit next;
                if (key < current.key) {
                    next = current.left.get(mark);
                } else {
                    next = current.right.get(mark);
                }
                
                if (next == null || mark[0]) {
                    break;
                }
                current = next;
            }
            
            boolean[] mark = new boolean[1];
            current.marked.get(mark);
            if (!mark[0]) {
                return current;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Unit[] parent = new Unit[1];
        Unit[] grandParent = new Unit[1];
        Unit current = find(key, parent, grandParent);
        
        return current.key != null && current.key == key;
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Unit[] parent = new Unit[1];
            Unit[] grandParent = new Unit[1];
            Unit current = find(key, parent, grandParent);
            
            if (current.key != null && current.key == key) {
                return false;
            }
            
            if (current.key == null) {
                Unit newUnit = new Unit(key, true);
                if (parent[0] == null) {
                    if (root.compareAndSet(current, newUnit)) {
                        return true;
                    }
                    continue;
                }
                Unit newInternal = createInternalNode(key, current.key);
                if (linkChild(parent[0], current, newInternal)) {
                    return true;
                }
                continue;
            }
            
            Unit newInternal = createInternalNode(key, current.key);
            if (parent[0] == null) {
                if (root.compareAndSet(current, newInternal)) {
                    return true;
                }
                continue;
            }
            
            if (linkChild(parent[0], current, newInternal)) {
                return true;
            }
        }
    }

    private Unit createInternalNode(int key1, int key2) {
        Unit newInternal = new Unit(null, false);
        Unit leaf1 = new Unit(key1, true);
        Unit leaf2 = new Unit(key2, true);
        
        if (key1 < key2) {
            newInternal.left.set(leaf1, false);
            newInternal.right.set(leaf2, false);
            newInternal.key = key2;
        } else {
            newInternal.left.set(leaf2, false);
            newInternal.right.set(leaf1, false);
            newInternal.key = key1;
        }
        
        return newInternal;
    }

    private boolean linkChild(Unit parent, Unit oldChild, Unit newChild) {
        if (parent.left.getReference() == oldChild) {
            return parent.left.compareAndSet(oldChild, newChild, false, false);
        } else {
            return parent.right.compareAndSet(oldChild, newChild, false, false);
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Unit[] parent = new Unit[1];
            Unit[] grandParent = new Unit[1];
            Unit current = find(key, parent, grandParent);
            
            if (current.key == null || current.key != key) {
                return false;
            }
            
            if (current.marked.compareAndSet(false, true, false, true)) {
                // Node has been marked
                
                if (parent[0] == null) {
                    Unit newUnit = new Unit(null, true);
                    root.compareAndSet(current, newUnit);
                    return true;
                }
                
                Unit sibling;
                boolean isLeftChild = (parent[0].left.getReference() == current);
                if (isLeftChild) {
                    sibling = parent[0].right.getReference();
                } else {
                    sibling = parent[0].left.getReference();
                }
                
                if (sibling == null) {
                    continue;
                }
                
                if (grandParent[0] == null) {
                    root.compareAndSet(parent[0], sibling);
                    return true;
                }
                
                if (grandParent[0].left.getReference() == parent[0]) {
                    grandParent[0].left.compareAndSet(parent[0], sibling, false, false);
                } else {
                    grandParent[0].right.compareAndSet(parent[0], sibling, false, false);
                }
                return true;
            }
        }
    }
}