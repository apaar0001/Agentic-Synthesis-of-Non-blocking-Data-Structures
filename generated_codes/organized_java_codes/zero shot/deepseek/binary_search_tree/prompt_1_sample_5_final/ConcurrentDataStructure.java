package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    
    private static final class Unit {
        Integer key;
        AtomicMarkableReference<Unit> left;
        AtomicMarkableReference<Unit> right;
        boolean isLeaf;
        
        Unit() {
            this.key = null;
            this.isLeaf = true;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final AtomicMarkableReference<Unit> root;
    
    public ConcurrentDataStructure() {
        root = new AtomicMarkableReference<>(new Unit(), false);
    }
    
    private Unit readRef(AtomicMarkableReference<Unit> ref) {
        return ref.getReference();
    }
    
    private boolean isMarked(AtomicMarkableReference<Unit> ref) {
        boolean[] markHolder = new boolean[1];
        ref.get(markHolder);
        return markHolder[0];
    }
    
    @Override
    public boolean contains(int k) {
        Unit current = readRef(root);
        
        while (current != null && !current.isLeaf) {
            if (isMarked(current.left) || isMarked(current.right)) {
                return false;
            }
            
            if (k < current.key) {
                current = readRef(current.left);
            } else {
                current = readRef(current.right);
            }
        }
        
        if (current == null) {
            return false;
        }
        
        return current.key != null && current.key == k;
    }
    
    @Override
    public boolean add(int k) {
        while (true) {
            Unit current = readRef(root);
            Unit parent = null;
            AtomicMarkableReference<Unit> parentRef = root;
            
            while (current != null && !current.isLeaf) {
                if (isMarked(current.left) || isMarked(current.right)) {
                    break;
                }
                
                parent = current;
                if (k < current.key) {
                    parentRef = current.left;
                    current = readRef(current.left);
                } else {
                    parentRef = current.right;
                    current = readRef(current.right);
                }
            }
            
            if (current == null) {
                continue;
            }
            
            if (current.isLeaf) {
                if (current.key == null) {
                    current.key = k;
                    return true;
                }
                
                if (current.key == k) {
                    return false;
                }
                
                int oldKey = current.key;
                
                Unit newLeaf = new Unit();
                newLeaf.isLeaf = true;
                newLeaf.key = k;
                
                Unit oldLeaf = new Unit();
                oldLeaf.isLeaf = true;
                oldLeaf.key = oldKey;
                
                Unit newInternal = new Unit();
                newInternal.isLeaf = false;
                
                if (k < oldKey) {
                    newInternal.key = oldKey;
                    newInternal.left.set(newLeaf, false);
                    newInternal.right.set(oldLeaf, false);
                } else {
                    newInternal.key = k;
                    newInternal.left.set(oldLeaf, false);
                    newInternal.right.set(newLeaf, false);
                }
                
                if (parentRef.compareAndSet(current, newInternal, false, false)) {
                    return true;
                }
                continue;
            }
        }
    }
    
    @Override
    public boolean remove(int k) {
        while (true) {
            Unit current = readRef(root);
            Unit parent = null;
            Unit grandParent = null;
            AtomicMarkableReference<Unit> parentRef = root;
            AtomicMarkableReference<Unit> grandParentRef = null;
            
            while (current != null && !current.isLeaf) {
                if (isMarked(current.left) || isMarked(current.right)) {
                    break;
                }
                
                grandParent = parent;
                grandParentRef = parentRef;
                parent = current;
                
                if (k < current.key) {
                    parentRef = current.left;
                    current = readRef(current.left);
                } else {
                    parentRef = current.right;
                    current = readRef(current.right);
                }
            }
            
            if (current == null) {
                continue;
            }
            
            if (current.isLeaf) {
                if (current.key == null || current.key != k) {
                    return false;
                }
                
                if (parent == null) {
                    if (current.left.compareAndSet(null, null, false, true)) {
                        // Node has been marked
                        return true;
                    }
                    continue;
                }
                
                if (current.left.compareAndSet(null, null, false, true)) {
                    // Node has been marked
                    
                    Unit sibling;
                    if (parent.left.getReference() == current) {
                        sibling = readRef(parent.right);
                    } else {
                        sibling = readRef(parent.left);
                    }
                    
                    if (grandParent == null) {
                        root.compareAndSet(parent, sibling, false, false);
                        return true;
                    }
                    
                    if (grandParent.left.getReference() == parent) {
                        grandParent.left.compareAndSet(parent, sibling, false, false);
                    } else {
                        grandParent.right.compareAndSet(parent, sibling, false, false);
                    }
                    return true;
                }
            }
        }
    }
}