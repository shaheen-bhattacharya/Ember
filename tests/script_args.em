// The `args` global holds command-line arguments after the script path.
// The test runner passes none, so here it is empty — but always an array.
print str(args);        // expect: []
print len(args);        // expect: 0

// It behaves like any other array value.
push(args, "synthetic");
print args[0];          // expect: synthetic
print len(args);        // expect: 1
