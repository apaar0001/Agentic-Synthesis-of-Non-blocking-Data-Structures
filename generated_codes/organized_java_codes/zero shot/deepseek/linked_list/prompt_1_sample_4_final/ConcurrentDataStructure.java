package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final class Element {
        final int value;
        final AtomicMarkableReference<Element> next;

        Element(int value) {
            this.value = value;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Element> first;

    public ConcurrentDataStructure() {
        first = new AtomicMarkableReference<>(null, false);
    }

    private boolean unlinkMarked(Element prev, Element curr) {
        boolean[] marked = {false};
        Element next = curr.next.get(marked);
        if (marked[0]) {
            if (prev == null) {
                return first.compareAndSet(curr, next, false, false);
            } else {
                return prev.next.compareAndSet(curr, next, false, false);
            }
        }
        return false;
    }

    @Override
    public boolean add(int key) {
        Element newElement = new Element(key);
        
        while (true) {
            Element curr = first.getReference();
            Element prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Element next = curr.next.get(marked);
                
                if (marked[0]) {
                    if (!unlinkMarked(prev, curr)) {
                        break;
                    }
                    curr = (prev == null) ? first.getReference() : prev.next.getReference();
                    continue;
                }
                
                if (curr.value == key) {
                    return false;
                }
                
                prev = curr;
                curr = next;
            }
            
            if (prev == null) {
                if (first.compareAndSet(null, newElement, false, false)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(null, newElement, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Element curr = first.getReference();
            Element prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Element next = curr.next.get(marked);
                
                if (marked[0]) {
                    if (!unlinkMarked(prev, curr)) {
                        break;
                    }
                    curr = (prev == null) ? first.getReference() : prev.next.getReference();
                    continue;
                }
                
                if (curr.value == key) {
                    if (curr.next.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        unlinkMarked(prev, curr);
                        return true;
                    } else {
                        break;
                    }
                }
                
                prev = curr;
                curr = next;
            }
            
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Element curr = first.getReference();
        
        while (curr != null) {
            boolean[] marked = {false};
            Element next = curr.next.get(marked);
            
            if (!marked[0] && curr.value == key) {
                return true;
            }
            
            curr = next;
        }
        
        return false;
    }
}