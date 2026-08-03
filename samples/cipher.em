// Caesar cipher: encode, decode, and crack. Shifting is chr/ord arithmetic;
// cracking tries all 26 shifts and keeps the one whose output looks most
// like English, scored by counting the nine most common letters.

fun shiftChar(c, k) {
  var code = ord(c);
  if (code >= 65 and code <= 90) return chr((code - 65 + k) % 26 + 65);
  if (code >= 97 and code <= 122) return chr((code - 97 + k) % 26 + 97);
  return c;  // spaces and punctuation pass through
}

fun caesar(s, k) {
  var out = "";
  for (var i = 0; i < len(s); i += 1) out += shiftChar(s[i], k);
  return out;
}

var message = "The quick brown fox jumps over the lazy dog";

// ROT13 is its own inverse.
var rot = caesar(message, 13);
print rot;
print caesar(rot, 13);
print "round-trips: " + str(caesar(rot, 13) == message);
print "";

// Crack a ciphertext without knowing the shift.
fun englishScore(s) {
  var common = "etaoinshr";
  var score = 0;
  for (var i = 0; i < len(s); i += 1) {
    if (indexOf(common, lower(s[i])) >= 0) score += 1;
  }
  return score;
}

var secret = caesar(message, 7);
print "ciphertext: " + secret;

var bestShift = 0;
var bestScore = -1;
for (var k = 0; k < 26; k += 1) {
  var score = englishScore(caesar(secret, k));
  if (score > bestScore) {
    bestScore = score;
    bestShift = k;
  }
}
print "cracked:    " + caesar(secret, bestShift);
print "shift found: " + str((26 - bestShift) % 26) +
    ", correct: " + str(caesar(secret, bestShift) == message);
