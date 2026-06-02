package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    // Lock-freedom test helpers (auto-injected)
    private static final java.util.concurrent.atomic.AtomicBoolean _lfVictimChosen = new java.util.concurrent.atomic.AtomicBoolean(false);
    private static final ThreadLocal<Integer> _lfOpCount = ThreadLocal.withInitial(() -> 0);
    private static final ThreadLocal<Boolean> _lfRetired =
            ThreadLocal.withInitial(() -> false);

    /**
     * Decide if the *current* thread should become the victim.
     *
     * Each call increments a per-thread operation counter. Once a thread
     * has executed more than 100 operations and no victim has been chosen,
     * it atomically claims the victim role and will then stall.
     */
    private static boolean _lfShouldStall() {
        int c = _lfOpCount.get() + 1;
        _lfOpCount.set(c);
        if (c > 100 && !_lfVictimChosen.get() && _lfVictimChosen.compareAndSet(false, true)) {
            return true;
        }
        return false;
    }

    
    private static class Node {
        final int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;
        
        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final AtomicMarkableReference<Node> root;
    
    public ConcurrentDataStructure() {
        root = new AtomicMarkableReference<>(null, false);
    }
    
    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node curr = root.getReference();
            Node prev = null;
            boolean[] prevMarked = new boolean[1];
            boolean[] currMarked = new boolean[1];
            boolean wentRight = false;
            
            while (curr != null) {
                if (curr.key == key) {
                    if (currMarked[0]) {
                        if (prev != null) {
                            if (wentRight) {
                                prev.right.compareAndSet(curr, curr.right.getReference(), false, false);
                            } else {
                                prev.left.compareAndSet(curr, curr.left.getReference(), false, false);
                            }
                        } else {
                            root.compareAndSet(curr, curr.left.getReference(), false, false);
                        }
                        break;
                    }
                    return false;
                }
                
                prev = curr;
                prevMarked[0] = currMarked[0];
                wentRight = key > curr.key;
                
                if (wentRight) {
                    curr = curr.right.get(currMarked);
                } else {
                    curr = curr.left.get(currMarked);
                }
                
                if (prevMarked[0]) {
                    if (wentRight) {
                        prev.right.compareAndSet(curr, curr, false, false);
                    } else {
                        prev.left.compareAndSet(curr, curr, false, false);
                    }
                    break;
                }
            }
            
            if (prev == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }
            
            if (prevMarked[0]) {
                continue;
            }
            
            Node newNode = new Node(key);
            if (wentRight) {
                if (prev.right.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            } else {
                if (prev.left.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node curr = root.getReference();
            Node prev = null;
            boolean[] prevMarked = new boolean[1];
            boolean[] currMarked = new boolean[1];
            boolean wentRight = false;
            
            while (curr != null) {
                if (curr.key == key) {
                    if (currMarked[0]) {
                        return false;
                    }
                    
                    if (curr.left.attemptMark(curr, true)) {
                        // Node has been marked
            // Lock-freedom victim stall injection (auto-injected)
            if (_lfShouldStall()) {
                System.err.println("LOG: Victim thread stalling inside remove()");
                try {
                    Thread.sleep(10_000);
                } catch (InterruptedException ignored) {
                }
                System.err.println("LOG: Victim resumed and retiring");
                _lfRetired.set(true);
                return false;
            }
                        if (prev == null) {
                            root.compareAndSet(curr, curr.left.getReference(), false, false);
                        } else {
                            if (wentRight) {
                                prev.right.compareAndSet(curr, curr.left.getReference(), false, false);
                            } else {
                                prev.left.compareAndSet(curr, curr.left.getReference(), false, false);
                            }
                        }
                        return true;
                    }
                    break;
                }
                
                prev = curr;
                prevMarked[0] = currMarked[0];
                wentRight = key > curr.key;
                
                if (wentRight) {
                    curr = curr.right.get(currMarked);
                } else {
                    curr = curr.left.get(currMarked);
                }
                
                if (prevMarked[0]) {
                    if (wentRight) {
                        prev.right.compareAndSet(curr, curr, false, false);
                    } else {
                        prev.left.compareAndSet(curr, curr, false, false);
                    }
                    break;
                }
            }
            
            if (curr == null) {
                return false;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node curr = root.getReference();
        boolean[] marked = new boolean[1];
        
        while (curr != null) {
            if (curr.key == key) {
                return !marked[0];
            }
            
            if (key > curr.key) {
                curr = curr.right.get(marked);
            } else {
                curr = curr.left.get(marked);
            }
        }
        
        return false;
    }
}