package gnu.getopt;

import com.example.utils.Getopt.LongOpt;

/**
 * Compatibility class that delegates to our Commons CLI implementation
 */
public class Getopt {
    private com.example.utils.Getopt delegate;
    
    public Getopt(String progname, String[] args, String optstring) {
        delegate = new com.example.utils.Getopt(progname, args, optstring);
    }
    
    public Getopt(String progname, String[] args, String optstring, LongOpt[] longopts) {
        delegate = new com.example.utils.Getopt(progname, args, optstring, longopts);
    }
    
    public int getopt() {
        return delegate.getopt();
    }
    
    public String getOptarg() {
        return delegate.getOptarg();
    }
} 