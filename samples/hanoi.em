// Towers of Hanoi with the pegs actually simulated: each move pops a disk
// and pushes it onto its destination, checking legality (never a larger
// disk on a smaller one) as it goes. The recursion is textbook; the arrays
// keep it honest.

var pegs = [[], [], []];
var moves = 0;
var violations = 0;

fun moveDisk(from, to) {
  var disk = pop(pegs[from]);
  var dest = pegs[to];
  if (len(dest) > 0 and dest[len(dest) - 1] < disk) violations += 1;
  push(dest, disk);
  moves += 1;
}

fun solve(n, from, to, via) {
  if (n == 0) return;
  solve(n - 1, from, via, to);
  moveDisk(from, to);
  solve(n - 1, via, to, from);
}

fun run(disks) {
  pegs = [[], [], []];
  for (var d = disks; d >= 1; d -= 1) push(pegs[0], d);
  moves = 0;
  violations = 0;
  solve(disks, 0, 2, 1);
}

// Show the moves for a small tower...
run(3);
print "3 disks: " + str(moves) + " moves, final peg " + str(pegs[2]);

// ...then verify the classic 2^n - 1 count on a bigger one.
var n = 16;
run(n);
var expected = 1;
for (var i = 0; i < n; i += 1) expected *= 2;
expected -= 1;
print str(n) + " disks: " + str(moves) + " moves";
print "expected 2^" + str(n) + " - 1 = " + str(expected) + ": " +
    str(moves == expected);
print "all on the last peg, in order: " +
    str(len(pegs[2]) == n and violations == 0);
