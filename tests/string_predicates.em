// contains, startsWith, and endsWith natives.
print contains("haystack", "hay");        // expect: true
print contains("haystack", "stack");      // expect: true
print contains("haystack", "sta");        // expect: true
print contains("haystack", "needle");     // expect: false
print contains("abc", "abc");             // expect: true
print contains("abc", "abcd");            // expect: false

print startsWith("ember.em", "ember");    // expect: true
print startsWith("ember.em", ".em");      // expect: false
print startsWith("abc", "abc");           // expect: true
print startsWith("abc", "abcd");          // expect: false

print endsWith("ember.em", ".em");        // expect: true
print endsWith("ember.em", "ember");      // expect: false
print endsWith("abc", "abc");             // expect: true
print endsWith("abc", "xabc");            // expect: false

// The empty string is a prefix, suffix, and substring of everything.
print contains("abc", "");                // expect: true
print startsWith("abc", "");              // expect: true
print endsWith("abc", "");                // expect: true
print startsWith("", "");                 // expect: true

// Composes with the toolkit: filter file names by extension.
var files = ["life.em", "notes.txt", "maze.em", "README.md"];
var scripts = [];
for (var i = 0; i < len(files); i += 1) {
  if (endsWith(files[i], ".em")) push(scripts, files[i]);
}
print join(scripts, " ");                 // expect: life.em maze.em

// Bad inputs answer nil.
print contains(5, "a");                   // expect: nil
print startsWith("a", 5);                 // expect: nil
print endsWith(nil, "a");                 // expect: nil
