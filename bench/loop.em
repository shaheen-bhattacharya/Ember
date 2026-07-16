// Tight numeric loop: stresses local variable access and the dispatch loop.
var start = clock();
var sum = 0;
for (var i = 0; i < 10000000; i = i + 1) {
  sum = sum + i;
}
var elapsed = clock() - start;
print "sum = " + str(sum);
print "elapsed_s " + str(elapsed);
