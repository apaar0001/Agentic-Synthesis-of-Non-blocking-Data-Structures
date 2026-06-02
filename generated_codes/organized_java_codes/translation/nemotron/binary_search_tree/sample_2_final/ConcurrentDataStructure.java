package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static final class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

        Node(int k) {
            this.key = k;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicMarkableReference<>(null, false);
    }

    @Override
    public boolean add(int key) {
        Node newNode = new Node(key);
        while (true) {
            boolean[] markHolder = new boolean[1];
            Node curr = root.getReference();
            boolean[] currMark = new boolean[1];
            Node pred = null;
            AtomicMarkableReference<Node> predRef = null;
            boolean wentLeft = false;

            // Get mark for root
            curr = root.get(currMark);
            boolean currMarked = currMark[0];

            while (curr != null) {
                if (currMarked) {
                    // Skip logically deleted node
                    wentLeft = key < curr.key;
                    predRef = wentLeft ? curr.left : curr.right;
                    curr = pred