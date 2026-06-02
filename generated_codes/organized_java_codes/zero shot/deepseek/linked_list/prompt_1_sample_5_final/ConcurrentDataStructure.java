package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final class Entry {
        final int key;
        final AtomicMarkableReference<Entry> next;
        
        Entry(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final AtomicMarkableReference<Entry> head;
    
    public ConcurrentDataStructure() {
        head = new AtomicMarkableReference<>(null, false);
    }
    
    @Override
    public boolean add(int key) {
        while (true) {
            Entry prev = null;
            Entry current = head.getReference();
            
            while (current != null) {
                boolean[] markHolder = new boolean[1];
                Entry next = current.next.get(markHolder);
                
                if (markHolder[0]) {
                    if (prev == null) {
                        head.compareAndSet(current, next, false, false);
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
                    if (head.compareAndSet(null, newEntry, false, false)) {
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
            Entry prev = null;
            Entry current = head.getReference();
            
            while (current != null) {
                boolean[] markHolder = new boolean[1];
                Entry next = current.next.get(markHolder);
                
                if (markHolder[0]) {
                    if (prev == null) {
                        head.compareAndSet(current, next, false, false);
                    } else {
                        prev.next.compareAndSet(current, next, false, false);
                    }
                    break;
                }
                
                if (current.key == key) {
                    if (current.next.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        if (prev == null) {
                            head.compareAndSet(current, next, false, false);
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
        Entry current = head.getReference();
        
        while (current != null) {
            boolean[] markHolder = new boolean[1];
            Entry next = current.next.get(markHolder);
            
            if (!markHolder[0] && current.key == key) {
                return true;
            }
            
            current = next;
        }
        
        return false;
    }
}