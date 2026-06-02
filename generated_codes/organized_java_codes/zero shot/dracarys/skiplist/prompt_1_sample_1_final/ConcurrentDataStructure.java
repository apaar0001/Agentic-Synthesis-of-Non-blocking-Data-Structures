package com.example.Sets;
import com.example.utils.SetADT;

import java.util.Random;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;

    private Node header;
    private int level;

    public ConcurrentDataStructure() {
        header = new Node(MAX_LEVEL);
        level = 1;
    }

    private int randomLevel() {
        int level = 1;
        Random random = new Random();
        while (random.nextDouble() < P && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private boolean search(int key) {
        Node current = header;
        for (int i = level; i >= 1; i--) {
            while (current.forward[i] != null && current.forward[i].key < key) {
                current = current.forward[i];
            }
        }
        current = current.forward[1];
        return current != null && current.key == key;
    }

    @Override
    public boolean add(int key) {
        if (search(key)) {
            return false;
        }
        Node[] update = new Node[MAX_LEVEL];
        Node current = header;
        for (int i = level; i >= 1; i--) {
            while (current.forward[i] != null && current.forward[i].key < key) {
                current = current.forward[i];
            }
            update[i] = current;
        }
        int lvl = randomLevel();
        if (lvl > level) {
            for (int i = level + 1; i <= lvl; i++) {
                update[i] = header;
            }
            level = lvl;
        }
        Node newNode = new Node(lvl);
        newNode.key = key;
        for (int i = 1; i <= lvl; i++) {
            newNode.forward[i] = update[i].forward[i];
            update[i].forward[i] = newNode;
        }
        return true;
    }

    @Override
    public boolean remove(int key) {
        Node[] update = new Node[MAX_LEVEL];
        Node current = header;
        for (int i = level; i >= 1; i--) {
            while (current.forward[i] != null && current.forward[i].key < key) {
                current = current.forward[i];
            }
            update[i] = current;
        }
        current = current.forward[1];
        if (current != null && current.key == key) {
            for (int i = 1; i <= level; i++) {
                if (update[i].forward[i] != current) {
                    break;
                }
                update[i].forward[i] = current.forward[i];
            }
            while (level > 1 && header.forward[level] == null) {
                level--;
            }
            return true;
        }
        return false;
    }

    @Override
    public boolean contains(int key) {
        return search(key);
    }

    private static class Node {
        int key;
        Node[] forward;

        Node(int level) {
            forward = new Node[level + 1];
        }
    }
}