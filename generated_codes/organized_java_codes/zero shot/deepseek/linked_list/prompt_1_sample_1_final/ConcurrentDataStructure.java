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

    private boolean physicalCleanup(Element prev, Element target) {
        boolean[] marked = {false};
        Element next = target.next.get(marked);
        if (!marked[0]) return false;
        
        if (prev == null) {
            return first.compareAndSet(target, next, false, false);
        } else {
            return prev.next.compareAndSet(target, next, false, false);
        }
    }

    @Override
    public boolean add(int key) {
        Element newElement = new Element(key);
        
        while (true) {
            Element current = first.getReference();
            Element prev = null;
            
            while (current != null) {
                boolean[] marked = {false};
                Element next = current.next.get(marked);
                
                if (marked[0]) {
                    physicalCleanup(prev, current);
                    break;
                }
                
                if (current.value == key) {
                    return false;
                }
                
                prev = current;
                current = next;
            }
            
            if (prev == null) {
                if (first.compareAndSet(current, newElement, false, false)) {
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
            Element current = first.getReference();
            Element prev = null;
            
            while (current != null) {
                boolean[] marked = {false};
                Element next = current.next.get(marked);
                
                if (marked[0]) {
                    physicalCleanup(prev, current);
                    break;
                }
                
                if (current.value == key) {
                    if (current.next.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        physicalCleanup(prev, current);
                        return true;
                    } else {
                        break;
                    }
                }
                
                prev = current;
                current = next;
            }
            
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Element current = first.getReference();
        
        while (current != null) {
            boolean[] marked = {false};
            Element next = current.next.get(marked);
            
            if (!marked[0] && current.value == key) {
                return true;
            }
            
            if (marked[0]) {
                current = next;
            } else {
                current = next;
            }
        }
        
        return false;
    }
}