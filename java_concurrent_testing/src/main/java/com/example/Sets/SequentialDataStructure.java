package com.example.Sets;

import com.example.utils.SetADT;

public class SequentialDataStructure implements SetADT {
    public SequentialDataStructure() {
    }

    @Override
    public boolean contains(int key) {
        return false;
    }

    @Override
    public boolean add(int key) {
        return true;
    }

    @Override
    public boolean remove(int key) {
        return true;
    }
}