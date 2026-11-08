#include "languages.hpp"

#include <array>
#include <string_view>

namespace nordend::formats::text {
namespace {

using nordend::text::Style;

// The word lists, one language at a time. Kept out of clang-format: one word per line would hide
// them.
// clang-format off

// ---------------------------------------------------------------------------------------------
// C and C++
// ---------------------------------------------------------------------------------------------

constexpr std::array<std::string_view, 1> kSlashComments{"//"};

constexpr auto kCppKeywords = std::to_array<std::string_view>({
    "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
    "_Noreturn", "_Pragma", "_Static_assert", "_Thread_local", "__asm__", "__attribute__",
    "__declspec", "__inline", "__restrict", "alignas", "alignof", "and", "and_eq", "asm",
    "break", "case", "catch", "class", "co_await", "co_return", "co_yield", "compl", "concept",
    "const", "const_cast", "consteval", "constexpr", "constinit", "continue", "decltype",
    "default", "delete", "do", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
    "final", "for", "friend", "goto", "if", "import", "inline", "module", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "operator", "or", "or_eq", "override", "private",
    "protected", "public", "register", "reinterpret_cast", "requires", "restrict", "return",
    "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "template", "this",
    "thread_local", "throw", "try", "typedef", "typeid", "typename", "union", "using",
    "virtual", "volatile", "while", "xor", "xor_eq",
});

constexpr auto kCppTypes = std::to_array<std::string_view>({
    "FILE", "array", "atomic", "auto", "bool", "byte", "char", "char16_t", "char32_t",
    "char8_t", "deque", "double", "float", "function", "int", "list", "long", "map",
    "optional", "pair", "set", "shared_ptr", "short", "signed", "span", "string",
    "string_view", "tuple", "unique_ptr", "unordered_map", "unordered_set", "unsigned",
    "variant", "vector", "void", "wchar_t", "weak_ptr",
});

constexpr auto kCppConstants = std::to_array<std::string_view>({
    "NULL", "false", "nullptr", "stderr", "stdout", "true",
});

constexpr auto kCppPrefixes = std::to_array<std::string_view>({"L", "U", "u", "u8"});

constexpr Syntax kCpp{
    .line_comments = kSlashComments,
    .block_open = "/*",
    .block_close = "*/",
    .quotes = "\"'",
    .string_prefixes = kCppPrefixes,
    .raw_strings = RawStrings::cpp,
    .preprocessor = true,
    .digit_separators = true,
    .keywords = kCppKeywords,
    .types = kCppTypes,
    .constants = kCppConstants,
    .camel_case_types = true,
    .suffix_t_types = true,
    .capitals = Style::function,
};

constexpr std::array<std::string_view, 2> kCMime{"text/x-csrc", "text/x-chdr"};
constexpr std::array<std::string_view, 2> kCppMime{"text/x-c++src", "text/x-c++hdr"};

// ---------------------------------------------------------------------------------------------
// Rust
// ---------------------------------------------------------------------------------------------

constexpr auto kRustKeywords = std::to_array<std::string_view>({
    "Self", "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum",
    "extern", "fn", "for", "if", "impl", "in", "let", "loop", "macro_rules", "match", "mod",
    "move", "mut", "pub", "ref", "return", "self", "static", "struct", "super", "trait", "type",
    "union", "unsafe", "use", "where", "while", "yield",
});

constexpr auto kRustTypes = std::to_array<std::string_view>({
    "bool", "char", "f32", "f64", "i128", "i16", "i32", "i64", "i8", "isize", "str", "u128",
    "u16", "u32", "u64", "u8", "usize",
});

constexpr auto kRustConstants = std::to_array<std::string_view>({
    "Err", "None", "Ok", "Some", "false", "true",
});

constexpr auto kRustPrefixes = std::to_array<std::string_view>({"b", "c"});

constexpr Syntax kRust{
    .line_comments = kSlashComments,
    .block_open = "/*",
    .block_close = "*/",
    .nested_comments = true,
    .quotes = "\"'",
    .multiline_quotes = "\"",
    .string_prefixes = kRustPrefixes,
    .raw_strings = RawStrings::rust,
    .rust_attributes = true,
    .bang_macros = true,
    .lifetimes = true,
    .keywords = kRustKeywords,
    .types = kRustTypes,
    .constants = kRustConstants,
    .camel_case_types = true,
    .capitals = Style::number,
};

constexpr std::array<std::string_view, 1> kRustMime{"text/rust"};
constexpr std::array<std::string_view, 1> kRustExtensions{".rs"};

// ---------------------------------------------------------------------------------------------
// Python
// ---------------------------------------------------------------------------------------------

constexpr std::array<std::string_view, 1> kHashComments{"#"};

constexpr auto kPythonKeywords = std::to_array<std::string_view>({
    "and", "as", "assert", "async", "await", "break", "class", "cls", "continue", "def", "del",
    "elif", "else", "except", "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "self", "try", "while", "with",
    "yield",
});

constexpr auto kPythonTypes = std::to_array<std::string_view>({
    "bool", "bytearray", "bytes", "complex", "dict", "float", "frozenset", "int", "list",
    "object", "set", "str", "tuple", "type",
});

constexpr auto kPythonConstants = std::to_array<std::string_view>({
    "Ellipsis", "False", "None", "NotImplemented", "True", "__file__", "__name__",
});

constexpr auto kPythonPrefixes = std::to_array<std::string_view>({
    "b", "br", "f", "fr", "r", "rb", "rf", "t", "tr", "rt", "u",
});

constexpr Syntax kPython{
    .line_comments = kHashComments,
    .quotes = "\"'",
    .triple_quotes = true,
    .string_prefixes = kPythonPrefixes,
    .prefix_any_case = true,
    .raw_letter = 'r',
    .decorator = '@',
    .keywords = kPythonKeywords,
    .types = kPythonTypes,
    .constants = kPythonConstants,
    .camel_case_types = true,
    .capitals = Style::number,
};

constexpr std::array<std::string_view, 2> kPythonMime{"text/x-python3", "text/x-python"};
constexpr std::array<std::string_view, 2> kPythonExtensions{".py", ".pyi"};
constexpr std::array<std::string_view, 2> kPythonInterpreters{"python", "pypy"};

// ---------------------------------------------------------------------------------------------
// JavaScript and TypeScript
// ---------------------------------------------------------------------------------------------

constexpr auto kJsKeywords = std::to_array<std::string_view>({
    "abstract", "as", "async", "await", "break", "case", "catch", "class", "const", "continue",
    "debugger", "declare", "default", "delete", "do", "else", "enum", "export", "extends",
    "finally", "for", "from", "function", "get", "if", "implements", "import", "in", "infer",
    "instanceof", "interface", "is", "keyof", "let", "namespace", "new", "of", "override",
    "private", "protected", "public", "readonly", "return", "satisfies", "set", "static",
    "super", "switch", "this", "throw", "try", "type", "typeof", "var", "void", "while", "with",
    "yield",
});

constexpr auto kJsTypes = std::to_array<std::string_view>({
    "any", "bigint", "boolean", "never", "number", "object", "string", "symbol", "unknown",
});

constexpr auto kJsConstants = std::to_array<std::string_view>({
    "Infinity", "NaN", "false", "null", "true", "undefined",
});

constexpr Syntax kJavaScript{
    .line_comments = kSlashComments,
    .block_open = "/*",
    .block_close = "*/",
    .quotes = "\"'`",
    .multiline_quotes = "`",
    .decorator = '@',
    .keywords = kJsKeywords,
    .types = kJsTypes,
    .constants = kJsConstants,
    .camel_case_types = true,
    .capitals = Style::number,
};

constexpr std::array<std::string_view, 2> kJsMime{"text/javascript", "application/javascript"};
constexpr std::array<std::string_view, 4> kJsExtensions{".js", ".mjs", ".cjs", ".jsx"};
constexpr std::array<std::string_view, 3> kJsInterpreters{"node", "deno", "bun"};
constexpr std::array<std::string_view, 2> kTsMime{"text/x-typescript", "application/x-typescript"};
constexpr std::array<std::string_view, 4> kTsExtensions{".ts", ".mts", ".cts", ".tsx"};

// ---------------------------------------------------------------------------------------------
// Lua and Luau
// ---------------------------------------------------------------------------------------------

constexpr std::array<std::string_view, 1> kDashComments{"--"};

constexpr auto kLuaKeywords = std::to_array<std::string_view>({
    "and", "break", "continue", "do", "else", "elseif", "end", "export", "for", "function",
    "goto", "if", "in", "local", "not", "or", "repeat", "return", "then", "type", "until",
    "while",
});

constexpr auto kLuaTypes = std::to_array<std::string_view>({
    "any", "boolean", "never", "number", "string", "thread", "unknown", "userdata",
});

constexpr auto kLuaConstants = std::to_array<std::string_view>({"false", "nil", "true"});

constexpr Syntax kLua{
    .line_comments = kDashComments,
    .lua_long_brackets = true,
    .quotes = "\"'`",
    .keywords = kLuaKeywords,
    .types = kLuaTypes,
    .constants = kLuaConstants,
    .camel_case_types = true,
    .capitals = Style::number,
};

constexpr std::array<std::string_view, 1> kLuaMime{"text/x-lua"};
constexpr std::array<std::string_view, 1> kLuaExtensions{".lua"};
constexpr std::array<std::string_view, 2> kLuaInterpreters{"lua", "luajit"};
constexpr std::array<std::string_view, 1> kLuauExtensions{".luau"};
constexpr std::array<std::string_view, 1> kLuauInterpreters{"luau"};

// clang-format on

constexpr std::array kLanguages{
    Language{.name = "C", .mime = kCMime, .syntax = &kCpp},
    Language{.name = "C++", .mime = kCppMime, .syntax = &kCpp},
    Language{.name = "Rust", .mime = kRustMime, .extensions = kRustExtensions, .syntax = &kRust},
    Language{.name = "Python",
             .mime = kPythonMime,
             .extensions = kPythonExtensions,
             .interpreters = kPythonInterpreters,
             .syntax = &kPython},
    Language{.name = "JavaScript",
             .mime = kJsMime,
             .extensions = kJsExtensions,
             .interpreters = kJsInterpreters,
             .syntax = &kJavaScript},
    Language{
        .name = "TypeScript", .mime = kTsMime, .extensions = kTsExtensions, .syntax = &kJavaScript},
    Language{.name = "Lua",
             .mime = kLuaMime,
             .extensions = kLuaExtensions,
             .interpreters = kLuaInterpreters,
             .syntax = &kLua},
    Language{.name = "Luau",
             .extensions = kLuauExtensions,
             .interpreters = kLuauInterpreters,
             .syntax = &kLua},
};

}  // namespace

std::span<const Language> all_languages() { return kLanguages; }

}  // namespace nordend::formats::text
