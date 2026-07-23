// Conway's Game of Life on a wrapping grid, written in Ember.
// Run with: ./ember samples/life.em

fun makeGrid(width, height) {
  var rows = [];
  for (var y = 0; y < height; y += 1) {
    var row = [];
    for (var x = 0; x < width; x += 1) {
      push(row, 0);
    }
    push(rows, row);
  }
  return rows;
}

fun neighbors(grid, x, y) {
  var height = len(grid);
  var width = len(grid[0]);
  var count = 0;
  for (var dy = -1; dy <= 1; dy += 1) {
    for (var dx = -1; dx <= 1; dx += 1) {
      if (dx == 0 and dy == 0) continue;
      var nx = (x + dx + width) % width;
      var ny = (y + dy + height) % height;
      count += grid[ny][nx];
    }
  }
  return count;
}

fun step(grid) {
  var height = len(grid);
  var width = len(grid[0]);
  var next = makeGrid(width, height);
  for (var y = 0; y < height; y += 1) {
    for (var x = 0; x < width; x += 1) {
      var n = neighbors(grid, x, y);
      if (grid[y][x] == 1 and (n == 2 or n == 3)) next[y][x] = 1;
      else if (grid[y][x] == 0 and n == 3) next[y][x] = 1;
    }
  }
  return next;
}

fun show(grid, generation) {
  print "generation " + str(generation) + ":";
  for (var y = 0; y < len(grid); y += 1) {
    var line = "";
    for (var x = 0; x < len(grid[y]); x += 1) {
      if (grid[y][x] == 1) line += "#";
      else line += ".";
    }
    print line;
  }
}

// Seed a glider.
var grid = makeGrid(10, 8);
grid[0][1] = 1;
grid[1][2] = 1;
grid[2][0] = 1;
grid[2][1] = 1;
grid[2][2] = 1;

for (var gen = 0; gen < 4; gen += 1) {
  show(grid, gen);
  grid = step(grid);
}
