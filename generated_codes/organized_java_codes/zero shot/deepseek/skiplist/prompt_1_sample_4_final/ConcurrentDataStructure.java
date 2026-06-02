package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_HEIGHT = 16;
    private static final double PROBABILITY = 0.5;
    
    private final Node header;
    private final AtomicInteger currentHeight;
    
    public ConcurrentDataStructure() {
        header = new Node(MAX_HEIGHT);
        currentHeight = new AtomicInteger(1);
    }
    
    @Override
    public boolean add(int key) {
        int topLevel = randomHeight();
        Node[] preds = new Node[MAX_HEIGHT];
        Node[] succs = new Node[MAX_HEIGHT];
        
        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }
            
            Node newNode = new Node(topLevel);
            newNode.key = key;
            
            for (int i = 0; i < topLevel; i++) {
                newNode.next[i] = succs[i];
            }
            
            if (!preds[0].next[0].compareAndSet(succs[0], newNode, false, false)) {
                continue;
            }
            
            for (int i = 1; i < topLevel; i++) {
                while (true) {
                    if (preds[i].next[i].compareAndSet(succs[i], newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_HEIGHT];
        Node[] succs = new Node[MAX_HEIGHT];
        
        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }
            
            Node nodeToRemove = succs[0];
            
            if (nodeToRemove.next[0].isMarked()) {
                return false;
            }
            
            boolean success = nodeToRemove.next[0].attemptMark(nodeToRemove.next[0].getReference(), true);
            if (success) {
                // Node has been marked
                find(key, preds, succs);
                return true;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_HEIGHT];
        Node[] succs = new Node[MAX_HEIGHT];
        return find(key, preds, succs);
    }
    
    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        Node pred = header;
        Node curr = null;
        Node succ = null;
        
        retry: for (int level = currentHeight.get() - 1; level >= 0; level--) {
            curr = pred.next[level].getReference();
            while (true) {
                succ = curr.next[level].get(marked);
                while (marked[0]) {
                    if (!pred.next[level].compareAndSet(curr, succ, false, false)) {
                        continue retry;
                    }
                    curr = pred.next[level].getReference();
                    succ = curr.next[level].get(marked);
                }
                if (curr.key < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
            preds[level] = pred;
            succs[level] = curr;
        }
        return curr != null && curr.key == key && !curr.next[0].isMarked();
    }
    
    private int randomHeight() {
        int height = 1;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && height < MAX_HEIGHT) {
            height++;
        }
        return height;
    }
    
    private static class Node {
        int key;
        AtomicMarkableReference<Node>[] next;
        
        Node(int height) {
            next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}