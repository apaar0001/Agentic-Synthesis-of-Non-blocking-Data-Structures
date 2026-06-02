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
            this.isLeaf = true;
            this.key = null;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final AtomicMarkableReference<Unit> rootRef;
    
    public ConcurrentDataStructure() {
        rootRef = new AtomicMarkableReference<>(new Unit(), false);
    }
    
    @Override
    public boolean contains(int key) {
        Unit current = rootRef.getReference();
        
        while (current != null && !current.isLeaf) {
            boolean[] markHolder = new boolean[1];
            Unit next;
            
            if (key < current.key) {
                next = current.left.get(markHolder);
            } else {
                next = current.right.get(markHolder);
            }
            
            if (markHolder[0]) {
                return false;
            }
            current = next;
        }
        
        return current != null && current.key != null && current.key == key;
    }
    
    @Override
    public boolean add(int key) {
        while (true) {
            Unit parent = null;
            Unit current = rootRef.getReference();
            AtomicMarkableReference<Unit> parentRef = rootRef;
            boolean isLeftChild = false;
            
            while (current != null && !current.isLeaf) {
                parent = current;
                parentRef = new AtomicMarkableReference<>(parent, false);
                
                boolean[] markHolder = new boolean[1];
                Unit next;
                
                if (key < current.key) {
                    next = current.left.get(markHolder);
                    if (markHolder[0]) {
                        continue;
                    }
                    current = next;
                    isLeftChild = true;
                } else {
                    next = current.right.get(markHolder);
                    if (markHolder[0]) {
                        continue;
                    }
                    current = next;
                    isLeftChild = false;
                }
            }
            
            if (current == null) continue;
            
            if (current.key == null) {
                Unit newLeaf = new Unit();
                newLeaf.key = key;
                if (parentRef.compareAndSet(current, newLeaf, false, false)) {
                    return true;
                }
                continue;
            }
            
            if (current.key == key) {
                return false;
            }
            
            int oldKey = current.key;
            
            Unit newLeaf = new Unit();
            newLeaf.key = key;
            
            Unit oldLeaf = new Unit();
            oldLeaf.key = oldKey;
            
            Unit newInternal = new Unit();
            newInternal.isLeaf = false;
            
            if (key < oldKey) {
                newInternal.key = oldKey;
                newInternal.left.set(newLeaf, false);
                newInternal.right.set(oldLeaf, false);
            } else {
                newInternal.key = key;
                newInternal.left.set(oldLeaf, false);
                newInternal.right.set(newLeaf, false);
            }
            
            if (parent == null) {
                if (rootRef.compareAndSet(current, newInternal, false, false)) {
                    return true;
                }
            } else {
                AtomicMarkableReference<Unit> childRef = isLeftChild ? parent.left : parent.right;
                if (childRef.compareAndSet(current, newInternal, false, false)) {
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        while (true) {
            Unit parent = null;
            Unit grandParent = null;
            Unit current = rootRef.getReference();
            AtomicMarkableReference<Unit> parentRef = rootRef;
            AtomicMarkableReference<Unit> grandParentRef = null;
            boolean isLeftChild = false;
            boolean parentIsLeftChild = false;
            
            while (current != null && !current.isLeaf) {
                grandParent = parent;
                grandParentRef = parentRef;
                parent = current;
                parentRef = new AtomicMarkableReference<>(parent, false);
                
                boolean[] markHolder = new boolean[1];
                Unit next;
                
                if (key < current.key) {
                    next = current.left.get(markHolder);
                    if (markHolder[0]) {
                        continue;
                    }
                    current = next;
                    isLeftChild = true;
                    parentIsLeftChild = (grandParent != null && grandParent.left.getReference() == parent);
                } else {
                    next = current.right.get(markHolder);
                    if (markHolder[0]) {
                        continue;
                    }
                    current = next;
                    isLeftChild = false;
                    parentIsLeftChild = (grandParent != null && grandParent.right.getReference() == parent);
                }
            }
            
            if (current == null) continue;
            
            if (current.key == null || current.key != key) {
                return false;
            }
            
            if (parent == null) {
                Unit newLeaf = new Unit();
                if (rootRef.compareAndSet(current, newLeaf, false, false)) {
                    // Node has been marked
                    return true;
                }
                continue;
            }
            
            AtomicMarkableReference<Unit> childRef = isLeftChild ? parent.left : parent.right;
            if (childRef.compareAndSet(current, current, false, true)) {
                // Node has been marked
                return true;
            }
        }
    }
}