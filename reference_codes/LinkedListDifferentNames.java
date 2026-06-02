package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Linked List with Alternative Naming Conventions.
 *
 * Functionally equivalent to Harris-Michael but uses distinct naming
 * throughout:
 * - Inner node class: "Element" (not "Node")
 * - Key field: "data" (not "key")
 * - Next pointer: "nextElement" (not "next")
 * - Head/tail sentinels: "anchor" / "boundary"
 * - Traversal helper: "ScanResult" with fields p/c (not Window pred/curr)
 * - Traversal method: "scan()" (not "find()")
 *
 * This variant is useful as a CodeBLEU diversity reference: same algorithm,
 * completely different identifier names throughout.
 *
 * Lock-freedom: all operations loop on CAS; no thread can block another.
 */
public class LinkedListDifferentNames implements SetADT {

    private static class Element {
        final int data;
        final AtomicMarkableReference<Element> nextElement;

        Element(int d) {
            this.data = d;
            this.nextElement = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Element anchor; // sentinel head: Integer.MIN_VALUE
    private final Element boundary; // sentinel tail: Integer.MAX_VALUE

    public LinkedListDifferentNames() {
        anchor = new Element(Integer.MIN_VALUE);
        boundary = new Element(Integer.MAX_VALUE);
        anchor.nextElement.set(boundary, false);
    }

    private static class ScanResult {
        final Element p, c; // predecessor, current

        ScanResult(Element p, Element c) {
            this.p = p;
            this.c = c;
        }
    }

    /**
     * CAS-based traversal returning (p, c) where c.data >= d.
     * Physically removes logically-marked elements encountered en route.
     */
    private ScanResult scan(int d) {
        Element p, c, s;
        boolean[] m = { false };
        retry: while (true) {
            p = anchor;
            c = p.nextElement.getReference();
            while (true) {
                s = c.nextElement.get(m);
                while (m[0]) {
                    if (!p.nextElement.compareAndSet(c, s, false, false))
                        continue retry;
                    c = s;
                    s = c.nextElement.get(m);
                }
                if (c.data >= d)
                    return new ScanResult(p, c);
                p = c;
                c = s;
            }
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            ScanResult res = scan(key);
            if (res.c.data == key)
                return false;
            Element e = new Element(key);
            e.nextElement.set(res.c, false);
            if (res.p.nextElement.compareAndSet(res.c, e, false, false))
                return true;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            ScanResult res = scan(key);
            if (res.c.data != key)
                return false;
            Element s = res.c.nextElement.getReference();
            // Logical deletion: mark res.c.nextElement
            if (!res.c.nextElement.compareAndSet(s, s, false, true))
                continue;
            // Node has been marked
            res.p.nextElement.compareAndSet(res.c, s, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] m = { false };
        Element c = anchor;
        while (c.data < key) {
            c = c.nextElement.getReference();
        }
        c.nextElement.get(m);
        return (c.data == key && !m[0]);
    }
}
