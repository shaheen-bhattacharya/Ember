// replace native: substitute every occurrence, left to right.
print replace("hello world", "o", "0");        // expect: hell0 w0rld
print replace("aaa", "a", "b");                // expect: bbb
print replace("banana", "na", "NA");           // expect: baNANA
print replace("nothing here", "xyz", "!");     // expect: nothing here

// Deleting and growing.
print replace("a-b-c", "-", "");               // expect: abc
print replace("x", "x", "xx");                 // expect: xx

// A replacement containing the needle doesn't recurse.
print replace("ab", "a", "aa");                // expect: aab

// Non-overlapping, left to right.
print replace("aaaa", "aa", "b");              // expect: bb

// Whole-string and empty-subject cases.
print replace("abc", "abc", "") + "!";         // expect: !
print replace("", "a", "b") + "!";             // expect: !

// Composes with the rest of the string toolkit.
print replace(upper("warn: disk"), "WARN", "ERROR");  // expect: ERROR: DISK
print len(replace("spaces in here", " ", ""));        // expect: 12

// Bad inputs answer nil.
print replace("abc", "", "x");                 // expect: nil
print replace(1, "a", "b");                    // expect: nil
print replace("abc", 1, "b");                  // expect: nil
print replace("abc", "a", 1);                  // expect: nil
