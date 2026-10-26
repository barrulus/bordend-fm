// Linked only into sanitizer builds (see NORDEND_SANITIZER in CMakeLists.txt). Both functions are
// weak hooks the sanitizer runtimes look up; a build without that sanitizer just ignores one.
//
// LeakSanitizer: fontconfig keeps 320 bytes of global state from its first FcFontSort until
// FcFini. The amount does not grow with more calls (checked with a standalone program,
// fontconfig 2.17.1), and calling FcFini would break any other fontconfig user in the process,
// so it is suppressed.
//
// ThreadSanitizer: Mesa's gallium driver runs a GL worker thread (gl0) and synchronises with it
// through code TSan does not instrument, so every GL program reports races inside libgallium
// (seen with Mesa 26.2.5 on radeonsi, also before any Nordend thread exists). Only reports with
// a libgallium frame are suppressed: races in our threads still show.

extern "C" const char*
__lsan_default_suppressions();  // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
extern "C" const char*
__tsan_default_suppressions();  // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)

extern "C" const char*
__lsan_default_suppressions() {  // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
    return "leak:libfontconfig.so\n";
}

extern "C" const char*
__tsan_default_suppressions() {  // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
    return "race:libgallium\n";
}
