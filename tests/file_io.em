// readFile and writeFile natives.
var path = "/tmp/ember_file_io_test.txt";

// Round-trip.
print writeFile(path, "line one\nline two\n");  // expect: true
var text = readFile(path);
print len(split(text, "\n"));                   // expect: 3
print split(text, "\n")[0];                     // expect: line one
print split(text, "\n")[1];                     // expect: line two

// Overwrite, not append.
print writeFile(path, "short");                 // expect: true
print readFile(path);                           // expect: short
print len(readFile(path));                      // expect: 5

// Escapes survive the trip byte for byte.
writeFile(path, "a\tb");
print len(readFile(path));                      // expect: 3

// The empty file.
print writeFile(path, "");                      // expect: true
print readFile(path) + "!";                     // expect: !

// Missing files answer nil; unwritable paths answer false.
print readFile("/no/such/dir/nothing.txt");     // expect: nil
print writeFile("/no/such/dir/nothing.txt", "x");  // expect: false

// Bad inputs answer nil.
print readFile(5);                              // expect: nil
print writeFile(path, 42);                      // expect: nil
print writeFile(nil, "x");                      // expect: nil
