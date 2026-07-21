// Allocates ~300k unique short-lived strings: stresses allocation, interning,
// and mark-sweep GC pause behavior. Run with EMBER_LOG_GC=1 to see
// collections. The loop lives in a function so it is eligible for tier-up.
fun churn(count) {
  var i = 0;
  while (i < count) {
    var s = "value-" + str(i);
    i = i + 1;
  }
  return i;
}

churn(1000);  // warm-up
var start = clock();
var total = churn(300000);
var elapsed = clock() - start;
print "strings = " + str(total);
print "elapsed_s " + str(elapsed);
