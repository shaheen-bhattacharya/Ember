// N-Queens: place n queens on an n x n board so none attack each other.
// Classic backtracking — one queen per row, an array used as an explicit
// stack of column choices, and diagonal checks via abs().

// A queen at (row, col) is safe against every queen already on the stack
// if no earlier queen shares its column or either diagonal.
fun safe(cols, row, col) {
  for (var r = 0; r < row; r += 1) {
    var c = cols[r];
    if (c == col) return false;
    if (abs(c - col) == row - r) return false;
  }
  return true;
}

var first = [];  // column choices of the first solution found

// Try every column in this row; recurse on the safe ones. Returns the
// number of complete solutions below this point.
fun search(cols, row, n) {
  if (row == n) {
    if (len(first) == 0) {
      for (var i = 0; i < n; i += 1) push(first, cols[i]);
    }
    return 1;
  }
  var count = 0;
  for (var col = 0; col < n; col += 1) {
    if (safe(cols, row, col)) {
      push(cols, col);
      count += search(cols, row + 1, n);
      pop(cols);
    }
  }
  return count;
}

fun printBoard(cols) {
  for (var r = 0; r < len(cols); r += 1) {
    var line = "";
    for (var c = 0; c < len(cols); c += 1) {
      if (cols[r] == c) line += " Q";
      else line += " .";
    }
    print line;
  }
}

var total = search([], 0, 8);
print "first 8-queens solution:";
printBoard(first);
print "";

// Solution counts follow a known sequence — check ours against it.
var expected = [2, 10, 4, 40, 92];
for (var n = 4; n <= 8; n += 1) {
  first = [];
  var count = search([], 0, n);
  var verdict = "WRONG";
  if (count == expected[n - 4]) verdict = "ok";
  print str(n) + " queens: " + str(count) + " solutions (" + verdict + ")";
}
