// Word frequency counting, written in Ember: split the text into words,
// sort them so duplicates are adjacent, then count runs.
// Run with: ./ember samples/wordfreq.em

var text = "the quick brown fox jumps over the lazy dog " +
           "the dog barks and the fox runs over the lazy dog";

fun countRuns(words) {
  var report = [];
  var i = 0;
  while (i < len(words)) {
    var word = words[i];
    var count = 0;
    while (i < len(words) and words[i] == word) {
      count += 1;
      i += 1;
    }
    push(report, word + ": " + str(count));
  }
  return report;
}

var words = sort(split(text, " "));
var lines = countRuns(words);
for (var i = 0; i < len(lines); i += 1) {
  print lines[i];
}
print "total words: " + str(len(words));
print "unique words: " + str(len(lines));
