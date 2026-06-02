package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Element {
        final int value;
        AtomicMarkableReference<Element> next;
        
        Element(int value) {
            this.value = value;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final AtomicMarkableReference<Element> first;
    
    public ConcurrentDataStructure() {
        first = new AtomicMarkableReference<>(null, false);
    }
    
    @Override
    public boolean add(int key) {
        Element newElement = new Element(key);
        
        while (true) {
            Element current = first.getReference();
            Element prev = null;
            
            while (current != null) {
                boolean[] markHolder = new boolean[1];
                Element next = current.next.get(markHolder);
                
                if (markHolder[0]) {
                    if (prev == null) {
                        if (first.compareAndSet(current, next, false, false)) {
                            current = next;
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (prev.next.compareAndSet(current, next, false, false)) {
                            current = next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (current.value == key) {
                    return false;
                }
                
                prev = current;
                current = next;
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
            Element current = first.getReference();
            Element prev = null;
            
            while (current != null) {
                boolean[] markHolder = new boolean[1];
                Element next = current.next.get(markHolder);
                
                if (markHolder[0]) {
                    if (prev == null) {
                        if (first.compareAndSet(current, next, false, false)) {
                            current = next;
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (prev.next.compareAndSet(current, next, false, false)) {
                            current = next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (current.value == key) {
                    if (prev == null) {
                        if (first.attemptMark(current, true)) {
                            // Node has been marked
                            return true;
                        }
                    } else {
                        if (prev.next.attemptMark(current, true)) {
                            // Node has been marked
                            return true;
                        }
                    }
                    break;
                }
                
                prev = current;
                current = next;
            }
            
            if (current == null) {
                return false;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        Element current = first.getReference();
        
        while (current != null) {
            boolean[] markHolder = new boolean[1];
            Element next = current.next.get(markHolder);
            
            if (!markHolder[0] && current.value == key) {
                return true;
            }
            
            current = next;
        }
        
        return false;
    }
}