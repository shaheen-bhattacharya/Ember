// Churn enough unique short-lived strings to force several GC cycles, then
// verify live data survived. Run with EMBER_LOG_GC=1 to watch collections.
var keep = "start" + str(0);
var i = 0;
while (i < 100000) {
  var garbage = "temp-" + str(i);
  i = i + 1;
}
print keep;             // expect: start0
print i;                // expect: 100000
print str(3.5);         // expect: 3.5
print str(true);        // expect: true
print str(nil) + "!";   // expect: nil!
