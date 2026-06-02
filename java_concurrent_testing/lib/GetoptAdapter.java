package com.example.utils;

import org.apache.commons.cli.*;
import java.util.HashMap;
import java.util.Map;

/**
 * Adapter class that emulates GNU Getopt using Apache Commons CLI
 */
public class GetoptAdapter {
    private CommandLine cmd;
    private Options options;
    private String[] args;
    private int currentOptionIndex = 0;
    private String currentArg = null;
    private Map<String, Character> longOptToVal = new HashMap<>();
    private Option[] processedOptions;
    
    public static class LongOpt {
        public static final int NO_ARGUMENT = 0;
        public static final int REQUIRED_ARGUMENT = 1;
        public static final int OPTIONAL_ARGUMENT = 2;
        
        private String name;
        private int hasArg;
        private Object valObj;
        private char val;
        
        public LongOpt(String name, int hasArg, Object valObj, char val) {
            this.name = name;
            this.hasArg = hasArg;
            this.valObj = valObj;
            this.val = val;
        }
        
        public String getName() {
            return name;
        }
        
        public int getHasArg() {
            return hasArg;
        }
        
        public char getVal() {
            return val;
        }
    }
    
    public GetoptAdapter(String progname, String[] args, String optstring, LongOpt[] longopts) {
        this.args = args;
        this.options = new Options();
        
        // Parse optstring (short options)
        for (int i = 0; i < optstring.length(); i++) {
            char c = optstring.charAt(i);
            if (Character.isLetter(c) || Character.isDigit(c)) {
                boolean hasArg = (i + 1 < optstring.length() && optstring.charAt(i + 1) == ':');
                Option option = Option.builder(String.valueOf(c))
                                      .hasArg(hasArg)
                                      .build();
                options.addOption(option);
                if (hasArg) i++; // Skip the colon
            }
        }
        
        // Parse longopts
        if (longopts != null) {
            for (LongOpt longopt : longopts) {
                if (longopt != null) {
                    Option.Builder optBuilder = Option.builder();
                    if (longopt.getVal() != '\0') {
                        optBuilder.option(String.valueOf(longopt.getVal()));
                    }
                    Option option = optBuilder.longOpt(longopt.getName())
                                    .hasArg(longopt.getHasArg() != LongOpt.NO_ARGUMENT)
                                    .build();
                    options.addOption(option);
                    longOptToVal.put(longopt.getName(), longopt.getVal());
                }
            }
        }
        
        try {
            CommandLineParser parser = new DefaultParser();
            this.cmd = parser.parse(options, args);
            this.processedOptions = cmd.getOptions();
        } catch (ParseException e) {
            System.err.println("Error parsing command line arguments: " + e.getMessage());
            this.processedOptions = new Option[0];
        }
    }
    
    public int getopt() {
        if (currentOptionIndex >= processedOptions.length) {
            return -1; // No more options
        }
        
        Option currentOption = processedOptions[currentOptionIndex++];
        currentArg = currentOption.getValue();
        
        // Return short option character if available, otherwise use mapped value for long option
        if (currentOption.getOpt() != null) {
            return currentOption.getOpt().charAt(0);
        } else if (currentOption.getLongOpt() != null && longOptToVal.containsKey(currentOption.getLongOpt())) {
            return longOptToVal.get(currentOption.getLongOpt());
        }
        
        return '?'; // Unknown option
    }
    
    public String getOptarg() {
        return currentArg;
    }
} 