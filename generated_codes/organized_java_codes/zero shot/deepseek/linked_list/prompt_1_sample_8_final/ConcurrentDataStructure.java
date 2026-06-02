package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Entry {
        final int key;
        final AtomicMarkableReference<Entry> next;
        
        Entry(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final AtomicMarkableReference<Entry> first;
    
    public ConcurrentDataStructure() {
        first = new AtomicMarkableReference<>(null, false);
    }
    
    @Override
    public boolean add(int key) {
        while (true) {
            Entry current = first.getReference();
            Entry prev = null;
            
            while (current != null) {
                boolean[] marked = {false};
                Entry next = current.next.get(marked);
                
                if (marked[0]) {
                    if (prev == null) {
                        first.compareAndSet(current, next, false, false);
                    } else {
                        prev.next.compareAndSet(current, next, false, false);
                    }
                    break;
                }
                
                if (current.key == key) {
                    return false;
                }
                
                prev = current;
                current = next;
            }
            
            if (current == null) {
                Entry newEntry = new Entry(key);
                if (prev == null) {
                    if (first.compareAndSet(null, newEntry, false, false)) {
                        return true;
                    }
                } else {
                    if (prev.next.compareAndSet(null, newEntry, false, false)) {
                        return true;
                    }
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        while (true) {
            Entry current = first.getReference();
            Entry prev = null;
            
            while (current != null) {
                boolean[] marked = {false};
                Entry next = current.next.get(marked);
                
                if (marked[0]) {
                    if (prev == null) {
                        first.compareAndSet(current, next, false, false);
                    } else {
                        prev.next.compareAndSet(current, next, false, false);
                    }
                    break;
                }
                
                if (current.key == key) {
                    if (current.next.attemptMark(next, true)) {
                        // Node has been marked
                        if (prev == null) {
                            first.compareAndSet(current, next, false, false);
                        } else {
                            prev.next.compareAndSet(current, next, false, false);
                        }
                        return true;
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
        Entry current = first.getReference();
        
        while (current != null) {
            boolean[] marked = {false};
            Entry next = current.next.get(marked);
            
            if (marked[0]) {
                current = next;
                continue;
            }
            
            if (current.key == key) {
                return true;
            }
            
            current = next;
        }
        return false;
    }
}