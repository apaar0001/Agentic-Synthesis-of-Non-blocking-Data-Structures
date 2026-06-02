package com.example.Sets;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements SetADT {
    private static class Unit {
        int key;
        Unit left;
        Unit right;
        boolean isLeaf;
        
        Unit() {
            this.isLeaf = true;
        }
    }
    
    private Unit root;
    
    public ConcurrentDataStructure() {
        initialize();
    }
    
    private void initialize() {
        root = new Unit();
    }
    
    @Override
    public boolean contains(int key) {
        Unit current = root;
        
        while (!current.isLeaf) {
            if (key < current.key) {
                current = current.left;
            } else {
                current = current.right;
            }
        }
        
        return current.key == key;
    }
    
    @Override
    public boolean add(int key) {
        Unit parent = null;
        Unit current = root;
        
        while (!current.isLeaf) {
            parent = current;
            if (key < current.key) {
                current = current.left;
            } else {
                current = current.right;
            }
        }
        
        if (current.key == key) {
            return false;
        }
        
        if (current.key == 0 && !contains(key)) {
            current.key = key;
            return true;
        }
        
        int oldKey = current.key;
        
        Unit newLeaf = new Unit();
        newLeaf.isLeaf = true;
        newLeaf.key = key;
        
        Unit oldLeaf = new Unit();
        oldLeaf.isLeaf = true;
        oldLeaf.key = oldKey;
        
        Unit newInternal = new Unit();
        newInternal.isLeaf = false;
        
        if (key < oldKey) {
            newInternal.key = oldKey;
            newInternal.left = newLeaf;
            newInternal.right = oldLeaf;
        } else {
            newInternal.key = key;
            newInternal.left = oldLeaf;
            newInternal.right = newLeaf;
        }
        
        if (parent == null) {
            root = newInternal;
        } else {
            if (parent.left == current) {
                parent.left = newInternal;
            } else {
                parent.right = newInternal;
            }
        }
        
        return true;
    }
    
    @Override
    public boolean remove(int key) {
        Unit parent = null;
        Unit grandParent = null;
        Unit current = root;
        
        while (!current.isLeaf) {
            grandParent = parent;
            parent = current;
            if (key < current.key) {
                current = current.left;
            } else {
                current = current.right;
            }
        }
        
        if (current.key != key) {
            return false;
        }
        
        if (parent == null) {
            current.key = 0;
            return true;
        }
        
        Unit sibling;
        if (parent.left == current) {
            sibling = parent.right;
        } else {
            sibling = parent.left;
        }
        
        if (grandParent == null) {
            root = sibling;
            return true;
        }
        
        if (grandParent.left == parent) {
            grandParent.left = sibling;
        } else {
            grandParent.right = sibling;
        }
        
        return true;
    }
}