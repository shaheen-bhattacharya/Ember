#pragma once

// Prints the interpreter's recorded profile (hotness, type feedback, call-site
// caches) for every executed function to stderr. Enabled by EMBER_PROFILE=1;
// runs at VM teardown while the heap is still intact.
void printProfile();
