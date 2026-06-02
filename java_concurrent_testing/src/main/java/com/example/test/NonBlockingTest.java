
package com.example.test;

import java.io.IOException;
import java.util.Random;

import com.example.Sets.*;
import com.example.utils.SetADT;
import com.example.utils.Tools;

import gnu.getopt.Getopt;
import gnu.getopt.LongOpt;

/**
 * Non-Blocking Test (Lock-Freedom / Victim Test) for concurrent data
 * structures.
 *
 * Runs the victim-stall-instrumented version of the concurrent set benchmark.
 * The ConcurrentDataStructure is pre-instrumented with victim-sleep logic
 * before
 * this test runs. A thread is elected as "victim" and stalled inside remove();
 * the test verifies that all remaining threads can still make progress
 * (lock-freedom guarantee).
 *
 * Uses RunOperationsNonBlocking + RunControllerNonBlocking (the updated
 * deterministic harness).
 */
public class NonBlockingTest {

    private static int numberOfThreads = 5;
    private static int maxRunningTime = 2;
    private static int addPercent = 40;
    private static int searchPercent = 20;
    private static int removePercent = 40;
    private static int keyRange = 100;
    private static int seed = 0;
    private static boolean testSanity = true;
    private static String setType = "ConcurrentDataStructure";
    private static int warmuptime = 2;
    private static double begin;
    private static double end;
    private static double throughput;
    private static double fairness;
    private static int[] results;
    private static int[][] sanityAdds;
    private static int[][] sanityRemoves;
    private static int[] presentKeys;
    private static SetADT set;

    private static void defineSet() {
        switch (setType) {
            case "ConcurrentDataStructure":
                set = new ConcurrentDataStructure();
                break;
            default:
                set = null;
                break;
        }
    }

    private static void initializeSet() {
        Random rd = new Random(0);
        // Initialize half of the keys into the set
        for (int i = 0; i < keyRange / 2;) {
            int key = rd.nextInt(keyRange);
            boolean added = set.add(key);
            if (added) {
                i++;
            }
            if (added && testSanity) {
                presentKeys[key]++;
            }
        }
    }

    private static void InitializeTest(String[] args) {
        LongOpt[] longopts = new LongOpt[11];

        longopts[0] = new LongOpt("help", LongOpt.NO_ARGUMENT, null, 'h');
        longopts[1] = new LongOpt("duration", LongOpt.REQUIRED_ARGUMENT, null, 'd');
        longopts[2] = new LongOpt("num-threads", LongOpt.REQUIRED_ARGUMENT, null, 'n');
        longopts[3] = new LongOpt("seed", LongOpt.REQUIRED_ARGUMENT, null, 's');
        longopts[4] = new LongOpt("search-fraction", LongOpt.REQUIRED_ARGUMENT, null, 'r');
        longopts[5] = new LongOpt("insert-update-fraction", LongOpt.REQUIRED_ARGUMENT, null, 'i');
        longopts[6] = new LongOpt("delete-fraction", LongOpt.REQUIRED_ARGUMENT, null, 'x');
        longopts[7] = new LongOpt("keyspace1-size", LongOpt.REQUIRED_ARGUMENT, null, 'k');
        longopts[8] = new LongOpt("algo", LongOpt.REQUIRED_ARGUMENT, null, 'a');
        longopts[9] = new LongOpt("sanity", LongOpt.REQUIRED_ARGUMENT, null, 't');
        longopts[10] = new LongOpt("warm", LongOpt.REQUIRED_ARGUMENT, null, 'w');

        Getopt g = new Getopt("", args, "hd:n:s:r:i:x:k:a:t:w:", longopts);
        int c;
        String arg = null;

        while ((c = g.getopt()) != -1) {
            switch (c) {
                case 'h':
                    helpUser();
                    break;

                case 'a':
                    setType = g.getOptarg();
                    break;

                case 'd':
                    arg = g.getOptarg();
                    maxRunningTime = Integer.parseInt(arg);
                    break;

                case 'n':
                    arg = g.getOptarg();
                    numberOfThreads = Integer.parseInt(arg);
                    break;

                case 's':
                    arg = g.getOptarg();
                    seed = Integer.parseInt(arg);
                    break;

                case 'r':
                    arg = g.getOptarg();
                    searchPercent = Integer.parseInt(arg);
                    break;

                case 'i':
                    arg = g.getOptarg();
                    addPercent = Integer.parseInt(arg);
                    break;

                case 't':
                    arg = g.getOptarg();
                    testSanity = Boolean.parseBoolean(arg);
                    break;

                case 'x':
                    arg = g.getOptarg();
                    removePercent = Integer.parseInt(arg);
                    break;

                case 'k':
                    arg = g.getOptarg();
                    keyRange = Integer.parseInt(arg);
                    break;

                case 'w':
                    arg = g.getOptarg();
                    warmuptime = Integer.parseInt(arg);
                    break;

                case '?':
                    System.err.println("Use -h or --help for help\n");
                    helpUser();
                    System.exit(0);
                default:
                    return;
            }
        }

        if ((addPercent + removePercent + searchPercent) > 100) {
            System.err.println("(addPercent+removePercent+searchPercent) > 100");
            System.exit(1);
        }

        results = new int[numberOfThreads];

        if (testSanity) {
            presentKeys = new int[keyRange];
            sanityAdds = new int[numberOfThreads][keyRange];
            sanityRemoves = new int[numberOfThreads][keyRange];
        }

        defineSet();
    }

    private static void warmupVM() {
        RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
        try {
            Thread[] threads = new Thread[numberOfThreads];

            for (int i = 0; i < threads.length; i++) {
                threads[i] = new Thread(
                        new RunOperationsNonBlocking(set, i, addPercent, removePercent, keyRange, results,
                                sanityAdds, sanityRemoves, false));
            }

            for (Thread thread : threads) {
                thread.start();
            }

            RunControllerNonBlocking.startFlag = true;
            Thread.sleep(warmuptime * 1000);
            RunControllerNonBlocking.stopFlag = true;

            for (Thread thread : threads) {
                thread.join();
            }

            System.gc();
        } catch (IOException | InterruptedException ex) {
            // Handle exceptions as needed.
        } finally {
            System.gc();
        }

        RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
    }

    private static void runNonBlockingCheck() {
        RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
        try {
            Thread[] threads = new Thread[numberOfThreads];

            for (int i = 0; i < threads.length; i++) {
                threads[i] = new Thread(
                        new RunOperationsNonBlocking(set, i, addPercent, removePercent, keyRange, results,
                                sanityAdds, sanityRemoves, true));
            }

            for (Thread thread : threads) {
                thread.start();
            }

            RunControllerNonBlocking.startFlag = true;
            Thread.sleep(maxRunningTime * 1000);
            RunControllerNonBlocking.stopFlag = true;

            for (Thread thread : threads) {
                thread.join();
            }

            System.gc();

            RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
        } catch (IOException | InterruptedException e) {
            // Handle exceptions as needed.
        }
        boolean failedSanity = false;
        for (int k = 0; k < keyRange; k++) {
            int keyAdded = presentKeys[k];
            int keyRemoved = 0;
            int InitialKeyAdded = keyAdded;
            for (int tid = 0; tid < numberOfThreads; tid++) {
                keyAdded += sanityAdds[tid][k];
                keyRemoved += sanityRemoves[tid][k];
            }

            if (set.contains(k)) {
                if (keyAdded != keyRemoved + 1) {
                    System.out.printf("\u001B[32mFirst Sanity Test failed at key %d, keyAdded = %d, keyRemoved = %d.\n",
                            k, keyAdded, keyRemoved);
                    System.out.printf("Initial Key Added %d\n", InitialKeyAdded);
                    failedSanity = true;
                }
            } else if (keyAdded != keyRemoved) {
                System.out.printf("\u001B[32mSecond Sanity Test failed at key %d, keyAdded = %d, keyRemoved = %d.\n",
                        k, keyAdded, keyRemoved);
                System.out.printf("Initial Key Added %d\n", InitialKeyAdded);
                failedSanity = true;
            }
        }

        if (!failedSanity) {
            System.out.println("Sanity Test Passed");
        } else {
            System.out.println("Sanity Test Failed");
        }
    }

    private static void runBenchmark() {
        double totalOps = 0, maxOps = 0, minOps;
        RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
        try {
            Thread[] threads = new Thread[numberOfThreads];

            for (int i = 0; i < threads.length; i++) {
                threads[i] = new Thread(
                        new RunOperationsNonBlocking(set, i, addPercent, removePercent, keyRange, results,
                                sanityAdds, sanityRemoves, false));
            }

            for (Thread thread : threads) {
                thread.start();
            }

            begin = System.nanoTime();
            RunControllerNonBlocking.startFlag = true;
            Thread.sleep(maxRunningTime * 1000);
            RunControllerNonBlocking.stopFlag = true;

            for (Thread thread : threads) {
                thread.join();
            }

            end = System.nanoTime();
            System.gc();

            RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
        } catch (IOException | InterruptedException e) {
            // Handle exceptions as needed.
        }

        double exactTime = (end - begin) * 1e-9;

        for (int i = 0; i < numberOfThreads; i++) {
            totalOps += results[i];
            maxOps = (maxOps < results[i]) ? results[i] : maxOps;
        }

        minOps = maxOps;
        for (int i = 0; i < numberOfThreads; i++) {
            minOps = (minOps > results[i]) ? results[i] : minOps;
        }

        throughput = totalOps / exactTime;
        fairness = Math.min((numberOfThreads * minOps) / totalOps, totalOps / (numberOfThreads * maxOps));
    }

    private static void helpUser() {
        String help = "Non-Blocking (Lock-Freedom Victim) Test\n" +
                "\nUsage:\n  NonBlockingTest [options...]\n" +
                "\nOptions:\n" +
                "  -h, --help\n        Print this message\n" +
                "  -a, --algo  <Algorithm> (default=" + setType + ")\n" +
                "        Available Algorithms <ConcurrentDataStructure>\n" +
                "  -t, --test-sanity <Boolean>\n        Sanity check (default=" + testSanity + ")\n" +
                "  -d, --duration <int>\n        Test duration in seconds (0=infinite, default=" + maxRunningTime
                + "s)\n" +
                "  -n, --num-threads <int>\n        Number of threads (default=" + numberOfThreads + ")\n" +
                "  -s, --seed <int>\n        RNG seed (0=time-based, default=" + seed + ")\n" +
                "  -r, --search-fraction <int>\n        Fraction of search operations (default=" + searchPercent
                + "%)\n" +
                "  -i, --insert-update-fraction <int>\n        Fraction of insert/add operations (default=" + addPercent
                + "%)\n" +
                "  -x, --delete-fraction <int>\n        Fraction of delete operations (default=" + removePercent
                + "%)\n" +
                "  -w, --warm <int>\n        JVM warm up time in seconds (default=" + warmuptime + "s)\n" +
                "  -k, --keyspace-size <int>\n       Number of possible keys (default=" + keyRange + ")\n";

        System.out.println(help);
        System.exit(0);
    }

    /**
     * Main entry point for the non-blocking (victim-stall) test.
     *
     * @param args command-line arguments
     */
    public static void main(String[] args) {
        try {
            if (args.length < 2) {
                System.err.println("To see the available options of parameters, type flag -h.");
            }
            InitializeTest(args);

            System.err.printf(
                    "The experiment: Algo:%s, Distribution: search %d insert %d delete %d, Duration(s):%d, Threads:%d, KeyRange(starting at 0):%d\n",
                    setType, searchPercent, addPercent, removePercent, maxRunningTime, numberOfThreads, keyRange);
            initializeSet();

            if (testSanity) {
                runNonBlockingCheck();
            } else {
                System.err.println("Starting warm up");

                long memTree = Tools.getMemUsed();
                Tools.cleanMem(memTree);
                memTree = Tools.getMemUsed();
                warmupVM();
                Tools.cleanMem(memTree);
                System.err.println("End of warm up phase");

                set = null;
                defineSet();
                Tools.cleanMem(memTree);
                memTree = Tools.getMemUsed();
                initializeSet();
                Tools.cleanMem(memTree);
                memTree = Tools.getMemUsed();
                runBenchmark();

                long memOnFinish = Tools.cleanMem(memTree);
                System.out.printf("Throughput = %.0f Ops/sec\n", throughput);
                System.out.printf("Fairness = %.0f percent\n", fairness * 100);
                System.out.printf("Memory-footprint of operations = %d bytes\n", memOnFinish);
            }
        } catch (Throwable t) {
            System.err.println("[NonBlockingTest] FATAL: Unhandled exception from generated ConcurrentDataStructure:");
            t.printStackTrace(System.err);
            System.out.println("Sanity Test Failed");
            System.exit(1);
        }
    }
}
