// env native: read environment variables.
print env("PATH") != nil;                     // expect: true
print len(env("PATH")) > 0;                   // expect: true
print env("EMBER_SURELY_UNSET_VAR_42");       // expect: nil
print env(5);                                 // expect: nil
print env(nil);                               // expect: nil

// exit native: stops the program immediately; nothing after it runs.
print "before exit";                          // expect: before exit
exit(0);
print "unreachable";
