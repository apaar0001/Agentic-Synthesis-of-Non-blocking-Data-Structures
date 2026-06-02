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
package com.example.test;

import java.io.IOException;
import org.apache.commons.math3.distribution.AbstractIntegerDistribution;
import org.apache.commons.math3.distribution.ZipfDistribution;
import java.util.Random;
import com.example.utils.SetADT;

/**
 * Executes operations on the concurrent set using primitive integers.
 */
public class RunOperations implements Runnable {

    boolean testSanity;
    int threadId;
    int addPercent;
    int removePercent;
    int keyRange;
    int numberOfOps;
    SetADT set;
    Random randOp;
    Random randKey;
    AbstractIntegerDistribution z;
    int[] results, numberOfAdd, numberOfRemove;
    int[][] sanityAdds, sanityRemoves;

    /**
     * Constructs a new RunOperations instance.
     *
     * @param s             the set to operate on
     * @param tId           the thread identifier
     * @param aP            percentage of add operations
     * @param rP            percentage of remove operations
     * @param kR            key range
     * @param results       an array to store the number of operations performed per
     *                      thread
     * @param sanityAdds    a 2D array tracking successful adds per thread for each
     *                      key
     * @param sanityRemoves a 2D array tracking successful removes per thread for
     *                      each key
     * @param testSanity    flag to determine whether to run in sanity mode
     * @throws IOException if an I/O error occurs
     */
    public RunOperations(SetADT s, int tId, int aP, int rP, int kR, int[] results, int[][] sanityAdds,
            int[][] sanityRemoves, boolean testSanity) throws IOException {
        this.testSanity = testSanity;
        this.threadId = tId;
        this.addPercent = aP;
        this.removePercent = rP;
        this.keyRange = kR;
        this.set = s;
        this.randOp = new Random(threadId);
        this.randKey = new Random(threadId);
        this.z = new ZipfDistribution(keyRange, 5.0);
        this.numberOfOps = 0;
        this.numberOfAdd = new int[kR];
        this.numberOfRemove = new int[kR];
        this.results = results;
        this.sanityAdds = sanityAdds;
        this.sanityRemoves = sanityRemoves;

    }

    private void benchMarkRun() {
        while (!RunController.startFlag)
            ;

        while (!RunController.stopFlag) {
            int chooseOperation = randOp.nextInt(100);
            int key = randKey.nextInt(keyRange);

            if (chooseOperation < addPercent) {
                set.add(key);
            } else if (chooseOperation < addPercent + removePercent) {
                set.remove(key);
            } else {
                set.contains(key);
            }

            numberOfOps++;
            RunController.globalOps.incrementAndGet();

        }

        results[threadId] = numberOfOps;
    }

    private void sanityRun() {
        while (!RunController.startFlag)
            ;

        while (!RunController.stopFlag) {
            int chooseOperation = randOp.nextInt(2);
            int key = randKey.nextInt(keyRange);

            if (chooseOperation == 1) {
                if (set.add(key)) {
                    numberOfAdd[key]++;
                    RunController.globalOps.incrementAndGet();
                } else if (set.remove(key)) {
                    numberOfRemove[key]++;
                    RunController.globalOps.incrementAndGet();
                }
            } else {
                if (set.remove(key)) {
                    numberOfRemove[key]++;
                    RunController.globalOps.incrementAndGet();
                } else if (set.add(key)) {
                    numberOfAdd[key]++;
                    RunController.globalOps.incrementAndGet();
                }
            }

        }

        for (int i = 0; i < keyRange; i++) {
            sanityAdds[threadId][i] += numberOfAdd[i];
            sanityRemoves[threadId][i] += numberOfRemove[i];
        }
    }

    @Override
    public void run() {
        if (testSanity) {
            sanityRun();
        } else {
            benchMarkRun();
        }
    }
}
