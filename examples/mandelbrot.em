// ASCII Mandelbrot set. Run with: ./ember examples/mandelbrot.em
// The escape-time loop lives in a function so it tiers up to the JIT;
// with EMBER_JIT=0 it renders identically, just slower.
fun escapeTime(x0, y0, limit) {
  var x = 0;
  var y = 0;
  var i = 0;
  while (x * x + y * y <= 4 and i < limit) {
    var xt = x * x - y * y + x0;
    y = 2 * x * y + y0;
    x = xt;
    i += 1;
  }
  return i;
}

fun renderRow(py) {
  var row = "";
  for (var px = 0; px < 64; px += 1) {
    var x0 = px / 64 * 3.2 - 2.3;
    var y0 = py / 20 * 2.2 - 1.1;
    var i = escapeTime(x0, y0, 40);
    if (i == 40) row += "#";
    else if (i > 6) row += "+";
    else if (i > 3) row += ".";
    else row += " ";
  }
  return row;
}

for (var py = 0; py < 20; py += 1) {
  print renderRow(py);
}
