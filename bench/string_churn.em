// Allocates ~300k unique short-lived strings: stresses allocation, interning,
// and mark-sweep GC pause behavior. Run with EMBER_LOG_GC=1 to see collections.
var start = clock();
var i = 0;
while (i < 300000) {
  var s = "value-" + str(i);
  i = i + 1;
}
var elapsed = clock() - start;
print "strings = " + str(i);
print "elapsed_s " + str(elapsed);
