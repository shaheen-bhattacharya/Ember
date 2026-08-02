// Maze generation by iterative backtracking: carve a random walk, retreat
// when boxed in, until every cell is reached. The frontier lives in an array
// used as an explicit stack (the VM caps call depth, and W*H cells of
// recursion would blow it). Randomness is a seeded Lehmer LCG, so the maze
// is the same on every run.

var W = 12;
var H = 8;

var seed = 20260802;
fun rnd(n) {
  // Lehmer LCG; 48271 * 2^31 stays exact in a 64-bit float.
  seed = seed * 48271 % 2147483647;
  return seed % n;
}

// The picture: a (2H+1) x (2W+1) grid of wall characters. Cell (x, y) sits
// at picture[2y+1][2x+1]; the shared wall between two cells sits midway.
var picture = [];
for (var r = 0; r < 2 * H + 1; r += 1) {
  var row = [];
  for (var c = 0; c < 2 * W + 1; c += 1) push(row, "#");
  push(picture, row);
}

var visited = [];
for (var i = 0; i < W * H; i += 1) push(visited, false);

var stack = [0];
visited[0] = true;
picture[1][1] = " ";
var carved = 1;

while (len(stack) > 0) {
  var cell = stack[len(stack) - 1];
  var x = cell % W;
  var y = floor(cell / W);

  // Unvisited neighbors, by cell index.
  var options = [];
  if (x > 0 and !visited[cell - 1]) push(options, cell - 1);
  if (x < W - 1 and !visited[cell + 1]) push(options, cell + 1);
  if (y > 0 and !visited[cell - W]) push(options, cell - W);
  if (y < H - 1 and !visited[cell + W]) push(options, cell + W);

  if (len(options) == 0) {
    pop(stack);  // dead end: retreat
  } else {
    var next = options[rnd(len(options))];
    var nx = next % W;
    var ny = floor(next / W);
    picture[2 * ny + 1][2 * nx + 1] = " ";   // the new cell
    picture[y + ny + 1][x + nx + 1] = " ";   // the wall between
    visited[next] = true;
    carved += 1;
    push(stack, next);
  }
}

// Doorways in the outer wall: enter top-left, exit bottom-right.
picture[0][1] = " ";
picture[2 * H][2 * W - 1] = " ";

for (var r = 0; r < len(picture); r += 1) {
  print join(picture[r], "");
}
print str(W) + "x" + str(H) + " maze, all " + str(carved) +
    " cells reachable: " + str(carved == W * H);
