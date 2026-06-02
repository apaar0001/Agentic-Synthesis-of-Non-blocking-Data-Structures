package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    
    private static class Unit {
        Integer key;
        Unit left;
        Unit right;
        boolean isLeaf;
        final AtomicMarkableReference<Unit> marked;
        
        Unit() {
            this.key = null;
            this.left = null;
            this.right = null;
            this.isLeaf = true;
            this.marked = new AtomicMarkableReference<>(null, false);
        }
        
        Unit(Integer key, Unit left, Unit right, boolean isLeaf) {
            this.key = key;
            this.left = left;
            this.right = right;
            this.isLeaf = isLeaf;
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final AtomicMarkableReference<Unit> rootRef;
    
    public ConcurrentDataStructure() {
        Unit initial = new Unit();
        rootRef = new AtomicMarkableReference<>(initial, false);
    }
    
    private Unit readRoot() {
        return rootRef.getReference();
    }
    
    private boolean casRoot(Unit expected, Unit update) {
        return rootRef.compareAndSet(expected, update, false, false);
    }
    
    @Override
    public boolean contains(int key) {
        Unit current = readRoot();
        
        while (!current.isLeaf) {
            boolean[] mark = new boolean[1];
            current.marked.get(mark);
            if (mark[0]) {
                return false;
            }
            
            if (key < current.key) {
                current = current.left;
            } else {
                current = current.right;
            }
            
            if (current == null) {
                return false;
            }
        }
        
        boolean[] mark = new boolean[1];
        current.marked.get(mark);
        if (mark[0]) {
            return false;
        }
        
        return current.key != null && current.key == key;
    }
    
    @Override
    public boolean add(int key) {
        while (true) {
            Unit parent = null;
            Unit current = readRoot();
            
            while (!current.isLeaf) {
                boolean[] mark = new boolean[1];
                current.marked.get(mark);
                if (mark[0]) {
                    break;
                }
                
                parent = current;
                if (key < current.key) {
                    current = current.left;
                } else {
                    current = current.right;
                }
                
                if (current == null) {
                    break;
                }
            }
            
            boolean[] mark = new boolean[1];
            current.marked.get(mark);
            if (mark[0]) {
                continue;
            }
            
            if (current.key == null) {
                Unit newLeaf = new Unit(key, null, null, true);
                if (parent == null) {
                    if (casRoot(current, newLeaf)) {
                        return true;
                    }
                    continue;
                }
                
                boolean[] parentMark = new boolean[1];
                parent.marked.get(parentMark);
                if (parentMark[0]) {
                    continue;
                }
                
                if (parent.left == current) {
                    if (parent.marked.compareAndSet(null, current, false, false)) {
                        parent.left = newLeaf;
                        parent.marked.set(null, false);
                        return true;
                    }
                } else {
                    if (parent.marked.compareAndSet(null, current, false, false)) {
                        parent.right = newLeaf;
                        parent.marked.set(null, false);
                        return true;
                    }
                }
                continue;
            }
            
            if (current.key == key) {
                return false;
            }
            
            int oldKey = current.key;
            Unit newLeaf = new Unit(key, null, null, true);
            Unit oldLeaf = new Unit(oldKey, null, null, true);
            Unit newInternal = new Unit(null, null, null, false);
            
            if (key < oldKey) {
                newInternal.left = newLeaf;
                newInternal.right = oldLeaf;
                newInternal.key = oldKey;
            } else {
                newInternal.left = oldLeaf;
                newInternal.right = newLeaf;
                newInternal.key = key;
            }
            
            if (parent == null) {
                if (casRoot(current, newInternal)) {
                    return true;
                }
                continue;
            }
            
            boolean[] parentMark = new boolean[1];
            parent.marked.get(parentMark);
            if (parentMark[0]) {
                continue;
            }
            
            if (parent.left == current) {
                if (parent.marked.compareAndSet(null, current, false, false)) {
                    parent.left = newInternal;
                    parent.marked.set(null, false);
                    return true;
                }
            } else {
                if (parent.marked.compareAndSet(null, current, false, false)) {
                    parent.right = newInternal;
                    parent.marked.set(null, false);
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        while (true) {
            Unit grandParent = null;
            Unit parent = null;
            Unit current = readRoot();
            
            while (!current.isLeaf) {
                boolean[] mark = new boolean[1];
                current.marked.get(mark);
                if (mark[0]) {
                    break;
                }
                
                grandParent = parent;
                parent = current;
                if (key < current.key) {
                    current = current.left;
                } else {
                    current = current.right;
                }
                
                if (current == null) {
                    break;
                }
            }
            
            boolean[] mark = new boolean[1];
            current.marked.get(mark);
            if (mark[0]) {
                return false;
            }
            
            if (current.key == null || current.key != key) {
                return false;
            }
            
            if (parent == null) {
                if (current.marked.compareAndSet(null, current, false, true)) {
                    // Node has been marked
                    Unit newLeaf = new Unit();
                    if (casRoot(current, newLeaf)) {
                        return true;
                    }
                }
                continue;
            }
            
            boolean[] parentMark = new boolean[1];
            parent.marked.get(parentMark);
            if (parentMark[0]) {
                continue;
            }
            
            Unit sibling;
            if (parent.left == current) {
                sibling = parent.right;
                if (current.marked.compareAndSet(null, current, false, true)) {
                    // Node has been marked
                    
                    if (grandParent == null) {
                        if (casRoot(parent, sibling)) {
                            return true;
                        }
                    } else {
                        boolean[] gpMark = new boolean[1];
                        grandParent.marked.get(gpMark);
                        if (gpMark[0]) {
                            continue;
                        }
                        
                        if (grandParent.left == parent) {
                            if (grandParent.marked.compareAndSet(null, parent, false, false)) {
                                grandParent.left = sibling;
                                grandParent.marked.set(null, false);
                                return true;
                            }
                        } else {
                            if (grandParent.marked.compareAndSet(null, parent, false, false)) {
                                grandParent.right = sibling;
                                grandParent.marked.set(null, false);
                                return true;
                            }
                        }
                    }
                }
            } else {
                sibling = parent.left;
                if (current.marked.compareAndSet(null, current, false, true)) {
                    // Node has been marked
                    
                    if (grandParent == null) {
                        if (casRoot(parent, sibling)) {
                            return true;
                        }
                    } else {
                        boolean[] gpMark = new boolean[1];
                        grandParent.marked.get(gpMark);
                        if (gpMark[0]) {
                            continue;
                        }
                        
                        if (grandParent.left == parent) {
                            if (grandParent.marked.compareAndSet(null, parent, false, false)) {
                                grandParent.left = sibling;
                                grandParent.marked.set(null, false);
                                return true;
                            }
                        } else {
                            if (grandParent.marked.compareAndSet(null, parent, false, false)) {
                                grandParent.right = sibling;
                                grandParent.marked.set(null, false);
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
}