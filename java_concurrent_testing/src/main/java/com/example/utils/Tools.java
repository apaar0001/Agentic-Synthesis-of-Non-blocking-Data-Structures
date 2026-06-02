/* 
 * Copyright (c) 2015-2016, Bapi Chatterjee
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list 
 * of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice, this 
 * list of conditions and the following disclaimer in the documentation and/or other 
 * materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, 
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
 * DAMAGE.
 */
package com.example.utils;

import java.util.Random;
import java.util.concurrent.ThreadLocalRandom;

/**
 * Utility class for common operations.
 */
public class Tools {

    /**
     * Returns the midpoint (average) of two integers.
     *
     * @param a first integer
     * @param b second integer
     * @return the midpoint of a and b
     */
    public static int midPoint(int a, int b) {
        return (a + b) / 2;
    }

    /**
     * Returns a random integer between two integers.
     * The result is in the range [min, max), meaning it includes min and excludes max.
     *
     * @param a first integer
     * @param b second integer
     * @return a random integer between a and b
     */
    public static int randPoint(int a, int b) {
        // Ensure that a is the lower bound and b is the upper bound.
        int lower = Math.min(a, b);
        int upper = Math.max(a, b);
        return ThreadLocalRandom.current().nextInt(lower, upper);
    }

    /**
     * Gets the current memory usage in bytes.
     *
     * @return the used memory in bytes
     */
    public static long getMemUsed() {
        long tot = Runtime.getRuntime().totalMemory();
        long free = Runtime.getRuntime().freeMemory();
        return tot - free;
    }

    /**
     * Returns a random integer in the range [min, max) using the provided Random instance.
     *
     * @param random the Random instance to use
     * @param min the minimum value (inclusive)
     * @param max the maximum value (exclusive)
     * @return a random integer between min (inclusive) and max (exclusive)
     */
    public static int randomInRange(Random random, int min, int max) {
        return random.nextInt(max - min) + min;
    }

    /**
     * Prints (or calculates) the difference in memory usage from a previous measurement.
     *
     * @param txt a text label for the measurement
     * @param prev the previous memory usage value
     * @return the difference between the current and previous memory usage
     */
    public static long printMemUsed(String txt, long prev) {
        long current = getMemUsed();
        // Uncomment the following line to actually print the difference:
        // System.err.println(txt + ": " + (current - prev));
        return current - prev;
    }

    /**
     * Attempts to clean memory by calling the garbage collector several times,
     * and returns the final difference in memory usage.
     *
     * @param prevMemUsed the previous memory usage value
     * @return the final difference in memory usage after cleaning
     */
    public static long cleanMem(long prevMemUsed) {
        long ret = 0;
        for (int i = 0; i < 5; i++) {
            ret = printMemUsed("MemTree", prevMemUsed);
            System.gc();
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        return ret;
    }
}
