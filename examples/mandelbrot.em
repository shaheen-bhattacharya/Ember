// ASCII Mandelbrot set. Run with: ./ember examples/mandelbrot.em
for (var py = 0; py < 20; py = py + 1) {
  var row = "";
  for (var px = 0; px < 64; px = px + 1) {
    var x0 = px / 64 * 3.2 - 2.3;
    var y0 = py / 20 * 2.2 - 1.1;
    var x = 0;
    var y = 0;
    var i = 0;
    while (x * x + y * y <= 4 and i < 40) {
      var xt = x * x - y * y + x0;
      y = 2 * x * y + y0;
      x = xt;
      i = i + 1;
    }
    if (i == 40) row = row + "#";
    else if (i > 6) row = row + "+";
    else if (i > 3) row = row + ".";
    else row = row + " ";
  }
  print row;
}
