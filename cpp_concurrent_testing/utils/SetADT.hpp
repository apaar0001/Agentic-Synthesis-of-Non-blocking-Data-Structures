#pragma once

class SetADT {
public:
    virtual ~SetADT() = default;

    /**
     * Checks if the set contains the given key.
     * @param key the integer key to search for
     * @return true if the key is present, false otherwise
     */
    virtual bool contains(int key) = 0;

    /**
     * Adds the given key to the set.
     * @param key the integer key to add
     * @return true if the key was added successfully, false if it was already present
     */
    virtual bool add(int key) = 0;

    /**
     * Removes the given key from the set.
     * @param key the integer key to remove
     * @return true if the key was removed, false if it was not found
     */
    virtual bool remove(int key) = 0;
};
