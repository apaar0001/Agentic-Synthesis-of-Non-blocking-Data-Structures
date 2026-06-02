package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicInteger;

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


    private static final int INITIAL_CAPACITY = 2; // must be power of two >=2
    private static final float LOAD_FACTOR = 0.75f;

    private final AtomicReference<Node[]> dir;
    private final AtomicInteger sizeCnt;

    public ConcurrentDataStructure() {
        this.dir = new AtomicReference<>(new Node[INITIAL_CAPACITY]);
        this.sizeCnt = new AtomicInteger(0);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int spread = spreadMix(key);
        while (true) {
            Node[] currentDir = dir.get();
            int k = 31 - Integer.numberOfLeadingZeros(currentDir.length);
            int shift = 32 - k;
            int bucket = spread >>> shift;

            Node pred = null;
            Node curr = currentDir[bucket];

            while (true) {
                if (curr == null) {
                    // end of list
                    break;
                }
                boolean marked = curr.next.isMarked();
                if (marked) {
                    // help remove marked node
                    Node next = curr.next.getReference();
                    if (pred == null) {
                        if (currentDir[bucket].compareAndSet(curr, next)) {
                            curr = next;
                            continue;
                        } else {
                            break outer; // restart add
                        }
                    } else {
                        if (pred.next.compareAndSet(curr, next, false, true)) {
                            curr = next;
                            continue;
                        } else {
                            break; // restart inner search
                        }
                    }
                }

                int currSpread = curr.spread;
                if (currSpread < spread) {
                    pred = curr;
                    curr = curr.next.getReference();
                } else if (currSpread == spread) {
                    if (curr.key == key) {
                        return false; // duplicate
                    }
                    if (curr.key < key) {
                        pred = curr;
                        curr = curr.next.getReference();
                    } else {
                        break; // insert before curr
                    }
                } else { // currSpread > spread
                    break; // insert before curr
                }
            }

            Node newNode = new Node(key, spread);
            newNode.next.set(curr, false);

            if (pred == null) {
                if (currentDir[bucket].compareAndSet(curr, newNode)) {
                    sizeCnt.incrementAndGet();
                    helpResize();
                    return true;
                } else {
                    break outer; // restart add
                }
            } else {
                if (pred.next.compareAndSet(curr, newNode, false, false)) {
                    sizeCnt.incrementAndGet();
                    helpResize();
                    return true;
                } else {
                    break outer; // restart add
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int spread = spreadMix(key);
        while (true) {
            Node[] currentDir = dir.get();
            int k = 31 - Integer.numberOfLeadingZeros(currentDir.length);
            int shift = 32 - k;
            int bucket = spread >>> shift;

            Node pred = null;
            Node curr = currentDir[bucket];

            while (true) {
                if (curr == null) {
                    return false; // not found
                }
                boolean marked = curr.next.isMarked();
                if (marked) {
                    // help remove marked node
                    Node next = curr.next.getReference();
                    if (pred == null) {
                        if (currentDir[bucket].compareAndSet(curr, next)) {
                            curr = next;
                            continue;
                        } else {
                            break outer; // restart remove
                        }
                    } else {
                        if (pred.next.compareAndSet(curr, next, false, true)) {
                            curr = next;
                            continue;
                        } else {
                            break; // restart inner search
                        }
                    }
                }

                int currSpread = curr.spread;
                if (currSpread < spread) {
                    pred = curr;
                    curr = curr.next.getReference();
                } else if (currSpread == spread) {
                    if (curr.key == key) {
                        // try to mark node
                        if (curr.next.attemptMark(curr.next.getReference(), true)) {
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
                            Node next = curr.next.getReference();
                            if (pred == null) {
                                currentDir[