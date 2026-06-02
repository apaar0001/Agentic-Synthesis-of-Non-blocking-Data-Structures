package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Element {
        final int value;
        final AtomicMarkableReference<Element> successor;

        Element(int value) {
            this.value = value;
            this.successor = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Element> first;

    public ConcurrentDataStructure() {
        first = new AtomicMarkableReference<>(null, false);
    }

    private boolean helpUnlink(Element prev, Element curr) {
        boolean[] marked = {false};
        Element next = curr.successor.get(marked);
        if (!marked[0]) return false;
        
        if (prev == null) {
            return first.compareAndSet(curr, next, false, false);
        } else {
            return prev.successor.compareAndSet(curr, next, false, false);
        }
    }

    @Override
    public boolean add(int key) {
        Element newElement = new Element(key);
        while (true) {
            Element current = first.getReference();
            Element prev = null;
            Element curr = current;
            
            while (curr != null) {
                boolean[] marked = {false};
                Element next = curr.successor.get(marked);
                
                if (marked[0]) {
                    helpUnlink(prev, curr);
                    if (prev == null) {
                        curr = first.getReference();
                        continue;
                    } else {
                        curr = prev.successor.getReference();
                        continue;
                    }
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
                if (prev.successor.compareAndSet(null, newElement, false, false)) {
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
            Element curr = current;
            
            while (curr != null) {
                boolean[] marked = {false};
                Element next = curr.successor.get(marked);
                
                if (marked[0]) {
                    helpUnlink(prev, curr);
                    if (prev == null) {
                        curr = first.getReference();
                        continue;
                    } else {
                        curr = prev.successor.getReference();
                        continue;
                    }
                }
                
                if (curr.value == key) {
                    if (curr.successor.attemptMark(next, true)) {
                        // Node has been marked
                        helpUnlink(prev, curr);
                        return true;
                    }
                    continue;
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
            Element next = curr.successor.get(marked);
            
            if (marked[0]) {
                helpUnlink(null, curr);
                curr = first.getReference();
                continue;
            }
            
            if (curr.value == key) {
                return true;
            }
            
            curr = next;
        }
        
        return false;
    }
}