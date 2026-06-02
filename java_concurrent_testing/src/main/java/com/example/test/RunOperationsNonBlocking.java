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
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import com.example.utils.SetADT;

/**
 * Executes operations on the victim-instrumented concurrent set using a
 * deterministic queue-based key schedule.
 *
 * Used exclusively by NonBlockingTest to run the lock-freedom victim-stall
 * benchmark. Coordinates via RunControllerNonBlocking.
 */
public class RunOperationsNonBlocking implements Runnable {

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

    private static final ConcurrentLinkedQueue<Integer> opQueue = new ConcurrentLinkedQueue<>();

    /**
     * Constructs a new RunOperationsNonBlocking instance.
     */
    public RunOperationsNonBlocking(SetADT s, int tId, int aP, int rP, int kR, int[] results, int[][] sanityAdds,
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

        // Initialize queue if empty (usually at the start of a new phase)
        synchronized (opQueue) {
            if (opQueue.isEmpty()) {
                List<Integer> keys = new ArrayList<>(keyRange * 1000);
                for (int k = 0; k < keyRange; k++) {
                    for (int i = 0; i < 1000; i++) {
                        keys.add(k);
                    }
                }
                Collections.shuffle(keys);
                opQueue.addAll(keys);
            }
        }
    }

    private void benchMarkRun() {
        while (!RunControllerNonBlocking.startFlag)
            ;

        while (!RunControllerNonBlocking.stopFlag) {
            Integer key = opQueue.poll();
            if (key == null)
                break;

            int chooseOperation = randOp.nextInt(100);
            if (chooseOperation < addPercent) {
                set.add(key);
            } else if (chooseOperation < addPercent + removePercent) {
                set.remove(key);
            } else {
                set.contains(key);
            }

            numberOfOps++;
            RunControllerNonBlocking.globalOps.incrementAndGet();

        }

        results[threadId] = numberOfOps;
    }

    private void sanityRun() {
        while (!RunControllerNonBlocking.startFlag)
            ;

        while (!RunControllerNonBlocking.stopFlag) {
            Integer key = opQueue.poll();
            if (key == null)
                break;

            int chooseOperation = randOp.nextInt(2);

            if (chooseOperation == 1) {
                if (set.add(key)) {
                    numberOfAdd[key]++;
                    RunControllerNonBlocking.globalOps.incrementAndGet();
                } else if (set.remove(key)) {
                    numberOfRemove[key]++;
                    RunControllerNonBlocking.globalOps.incrementAndGet();
                }
            } else {
                if (set.remove(key)) {
                    numberOfRemove[key]++;
                    RunControllerNonBlocking.globalOps.incrementAndGet();
                } else if (set.add(key)) {
                    numberOfAdd[key]++;
                    RunControllerNonBlocking.globalOps.incrementAndGet();
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
