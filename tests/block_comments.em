// Block comments: /* ... */, nestable.
/* a whole-line comment */
print 1; /* trailing */ // expect: 1
print /* inline */ 2;   // expect: 2

/*
 * spanning
 * several lines
 */
print 3;                // expect: 3

// Nesting: commenting out code that already has a block comment.
/*
print "dead";
/* inner */
print "still dead";
*/
print 4;                // expect: 4

// Comment markers inside strings are just characters.
print "/* not a comment */";  // expect: /* not a comment */

// Division and multiplication still lex normally next to comments.
print 10 / 2 /* five */ * 3;  // expect: 15
