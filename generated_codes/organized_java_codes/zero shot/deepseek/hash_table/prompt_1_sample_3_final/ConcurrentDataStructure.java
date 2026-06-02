package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    
    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;
        
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
        
        Node(int key, Node nextNode) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(nextNode, false);
        }
    }
    
    private final int capacity;
    private final Node[] buckets;
    
    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.buckets = new Node[capacity];
    }
    
    private int computeIndex(int key) {
        return Math.abs(key) % capacity;
    }
    
    @Override
    public boolean add(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node head = buckets[index];
            if (head == null) {
                Node newNode = new Node(key);
                if (casBucket(index, null, newNode)) {
                    return true;
                }
                continue;
            }
            
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                
                if (marked[0]) {
                    if (prev == null) {
                        if (casBucket(index, curr, next)) {
                            curr = buckets[index];
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (prev.next.compareAndSet(curr, next, false, false)) {
                            curr = next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                prev = curr;
                curr = next;
            }
            
            Node newNode = new Node(key, head);
            if (prev == null) {
                if (casBucket(index, head, newNode)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node head = buckets[index];
            if (head == null) {
                return false;
            }
            
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                
                if (marked[0]) {
                    if (prev == null) {
                        if (casBucket(index, curr, next)) {
                            curr = buckets[index];
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (prev.next.compareAndSet(curr, next, false, false)) {
                            curr = next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (curr.key == key) {
                    if (curr.next.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        if (prev == null) {
                            casBucket(index, curr, next);
                        } else {
                            prev.next.compareAndSet(curr, next, false, false);
                        }
                        return true;
                    }
                    break;
                }
                
                prev = curr;
                curr = next;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        int index = computeIndex(key);
        Node curr = buckets[index];
        
        while (curr != null) {
            boolean[] marked = {false};
            Node next = curr.next.get(marked);
            
            if (!marked[0] && curr.key == key) {
                return true;
            }
            
            curr = next;
        }
        
        return false;
    }
    
    private boolean casBucket(int index, Node expected, Node update) {
        return unsafeCompareAndSetObject(buckets, index, expected, update);
    }
    
    private static final sun.misc.Unsafe UNSAFE;
    private static final long BUCKETS_OFFSET;
    
    static {
        try {
            sun.misc.Unsafe unsafe = null;
            try {
                java.lang.reflect.Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                unsafe = (sun.misc.Unsafe) f.getReference();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            UNSAFE = unsafe;
            BUCKETS_OFFSET = UNSAFE.arrayBaseOffset(Node[].class);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
    
    private boolean unsafeCompareAndSetObject(Node[] array, int index, Node expected, Node update) {
        long offset = BUCKETS_OFFSET + (index * UNSAFE.arrayIndexScale(Node[].class));
        return UNSAFE.compareAndSwapObject(array, offset, expected, update);
    }
}