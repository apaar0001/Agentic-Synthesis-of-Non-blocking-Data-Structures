package gnu.getopt;

/**
 * Compatibility LongOpt class that extends our Commons CLI implementation
 */
public class LongOpt extends com.example.utils.Getopt.LongOpt {
    public static final int NO_ARGUMENT = com.example.utils.Getopt.LongOpt.NO_ARGUMENT;
    public static final int REQUIRED_ARGUMENT = com.example.utils.Getopt.LongOpt.REQUIRED_ARGUMENT;
    public static final int OPTIONAL_ARGUMENT = com.example.utils.Getopt.LongOpt.OPTIONAL_ARGUMENT;
    
    public LongOpt(String name, int hasArg, Object valObj, char val) {
        super(name, hasArg, valObj, val);
    }
} 