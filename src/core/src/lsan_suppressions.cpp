// Linked only into sanitizer builds (see NORDEND_SANITIZER in CMakeLists.txt).
//
// fontconfig keeps 320 bytes of global state from its first FcFontSort until FcFini. The amount
// does not grow with more calls (checked with a standalone program, fontconfig 2.17.1), and
// calling FcFini would break any other fontconfig user in the process, so it is suppressed.

extern "C" const char* __lsan_default_suppressions();  // NOLINT(bugprone-reserved-identifier)

extern "C" const char* __lsan_default_suppressions() {  // NOLINT(bugprone-reserved-identifier)
    return "leak:libfontconfig.so\n";
}
