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
    
    private final Entry head;
    
    public ConcurrentDataStructure() {
        head = new Entry(Integer.MIN_VALUE);
    }
    
    private Entry find(int key, AtomicMarkableReference<Entry>[] refs) {
        retry: while (true) {
            Entry prev = head;
            Entry curr = prev.next.getReference();
            
            while (curr != null) {
                boolean[] markHolder = new boolean[1];
                Entry next = curr.next.get(markHolder);
                
                if (markHolder[0]) {
                    if (!prev.next.compareAndSet(curr, next, false, false)) {
                        continue retry;
                    }
                    curr = next;
                    continue;
                }
                
                if (curr.key >= key) {
                    refs[0] = prev.next;
                    refs[1] = curr.next;
                    return curr;
                }
                
                prev = curr;
                curr = next;
            }
            
            refs[0] = prev.next;
            refs[1] = null;
            return null;
        }
    }
    
    @Override
    public boolean add(int key) {
        AtomicMarkableReference<Entry>[] refs = new AtomicMarkableReference[2];
        
        while (true) {
            Entry curr = find(key, refs);
            
            if (curr != null && curr.key == key) {
                return false;
            }
            
            Entry newEntry = new Entry(key);
            newEntry.next.set(curr, false);
            
            if (refs[0].compareAndSet(curr, newEntry, false, false)) {
                return true;
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        AtomicMarkableReference<Entry>[] refs = new AtomicMarkableReference[2];
        
        while (true) {
            Entry curr = find(key, refs);
            
            if (curr == null || curr.key != key) {
                return false;
            }
            
            Entry next = curr.next.getReference();
            if (curr.next.compareAndSet(next, next, false, true)) {
                // Node has been marked
                refs[0].compareAndSet(curr, next, false, false);
                return true;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        AtomicMarkableReference<Entry>[] refs = new AtomicMarkableReference[2];
        Entry curr = find(key, refs);
        return curr != null && curr.key == key && !curr.next.isMarked();
    }
}