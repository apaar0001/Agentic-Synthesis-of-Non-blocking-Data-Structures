package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free BST with Alternative Naming Conventions.
 *
 * Distinct naming throughout for CodeBLEU diversity:
 * - Node class: "TreeNode"
 * - Key field: "data" (not "key" or "k")
 * - Mark flag: "active" (true = present, false = deleted — inverted logic)
 * - Children: "leftNode" / "rightNode"
 * - Root: "top"
 * - Parameter: "val" in add/remove/contains
 *
 * Note on "active" semantics: active=true means node is in the set,
 * active=false means logically deleted. This is the inverse of the "marked"
 * convention used in refs 2-4.
 *
 * Lock-freedom: all operations use CAS; no thread holds any lock.
 */
public class BSTDifferentNames implements SetADT {

    private static class TreeNode {
        final int data;
        final AtomicBoolean active; // true = in set, false = deleted
        final AtomicReference<TreeNode> leftNode, rightNode;

        TreeNode(int d) {
            data = d;
            active = new AtomicBoolean(true); // New nodes start active
            leftNode = new AtomicReference<>(null);
            rightNode = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<TreeNode> top = new AtomicReference<>(null);

    @Override
    public boolean add(int key) {
        while (true) {
            TreeNode n = new TreeNode(key);
            if (top.compareAndSet(null, n))
                return true;
            TreeNode c = top.get();
            while (true) {
                if (c.data == key) {
                    // Re-activate a deleted node
                    if (c.active.compareAndSet(false, true))
                        return true;
                    return false; // Already active
                }
                AtomicReference<TreeNode> next = (key < c.data) ? c.leftNode : c.rightNode;
                TreeNode t = next.get();
                if (t == null) {
                    if (next.compareAndSet(null, n))
                        return true;
                    t = next.get();
                }
                c = t;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        TreeNode c = top.get();
        while (c != null) {
            if (c.data == key) {
                // Deactivate: CAS active from true → false
                if (c.active.compareAndSet(true, false)) {
                    // Node has been marked
                    return true;
                }
                return false; // Already inactive
            }
            c = (key < c.data) ? c.leftNode.get() : c.rightNode.get();
        }
        return false;
    }

    @Override
    public boolean contains(int key) {
        TreeNode c = top.get();
        while (c != null) {
            if (c.data == key)
                return c.active.get();
            c = (key < c.data) ? c.leftNode.get() : c.rightNode.get();
        }
        return false;
    }
}
