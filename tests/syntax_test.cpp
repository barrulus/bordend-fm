#include "nordend/formats/text/syntax.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/text/text.hpp"

namespace text = nordend::text;
namespace family = nordend::formats::text;
using Lines = std::vector<std::string>;

namespace {

// A line with its coloured pieces marked: {k:if} keyword, {t:} type, {s:} string, {n:} number or
// constant, {c:} comment, {f:} function or macro, {a:} added, {r:} removed.
std::string marked(const std::string& line, const std::vector<text::Span>& spans) {
    static constexpr std::string_view kLetters = "ktsncfar";
    std::string out;
    std::size_t at = 0;
    for (const text::Span& span : spans) {
        REQUIRE(span.start >= at);
        REQUIRE(span.start + span.length <= line.size());
        out += line.substr(at, span.start - at);
        out += '{';
        out += kLetters[static_cast<std::size_t>(span.style)];
        out += ':';
        out += line.substr(span.start, span.length);
        out += '}';
        at = span.start + span.length;
    }
    out += line.substr(at);
    return out;
}

// lines coloured as the language of name and mime; each line marked.
Lines colour(const Lines& lines, std::string_view mime, std::string_view name = "file") {
    text::Text text;
    text.lines = lines;
    family::colour(text, name, mime);
    Lines out;
    for (std::size_t i = 0; i < text.lines.size(); ++i) {
        out.push_back(
            marked(text.lines[i], text.spans.empty() ? std::vector<text::Span>{} : text.spans[i]));
    }
    return out;
}

Lines cpp(const Lines& lines) { return colour(lines, "text/x-c++src"); }

}  // namespace

TEST_CASE("syntax: the language from the type, the name or the first line") {
    const auto name_of = [](std::string_view name, std::string_view mime,
                            std::string_view first = {}) -> std::string {
        const family::Language* language = family::language_for(name, mime, first);
        return language == nullptr ? "" : std::string{family::language_name(*language)};
    };
    CHECK(name_of("a.c", "text/x-csrc") == "C");
    CHECK(name_of("a.h", "text/x-chdr") == "C");
    CHECK(name_of("a.cpp", "text/x-c++src") == "C++");
    CHECK(name_of("a.hpp", "text/x-c++hdr") == "C++");
    CHECK(name_of("notes.txt", "text/plain").empty());
    CHECK(name_of("notes.txt", "text/plain", "#!/bin/frobnicate").empty());

    text::Text plain;
    plain.lines = {"int x;"};
    family::colour(plain, "notes.txt", "text/plain");
    CHECK(plain.language.empty());
    CHECK(plain.spans.empty());
    text::Text code;
    code.lines = {"int x;", "", "y"};
    family::colour(code, "a.c", "text/x-csrc");
    CHECK(code.language == "C");
    CHECK(code.spans.size() == 3);  // one per line, empty ones too
}

TEST_CASE("syntax: C++ words") {
    CHECK(cpp({"int main(int argc, char** argv) {"}) ==
          Lines{"{t:int} {f:main}({t:int} argc, {t:char}** argv) {"});
    CHECK(cpp({"    return std::string_view{name}.size();"}) ==
          Lines{"    {k:return} std::{t:string_view}{name}.{f:size}();"});
    CHECK(cpp({"constexpr std::uint32_t kMagic = 0x4E50;"}) ==
          Lines{"{k:constexpr} std::{t:uint32_t} kMagic = {n:0x4E50};"});
    CHECK(cpp({"if (ptr == nullptr && !done) throw Error{};"}) ==
          Lines{"{k:if} (ptr == {n:nullptr} && !done) {k:throw} {t:Error}{};"});
    // Macros in capitals, CamelCase types, names called with spaces before the parenthesis.
    CHECK(cpp({"TEST_CASE (\"x\") { Registry r; EINTR; }"}) ==
          Lines{"{f:TEST_CASE} ({s:\"x\"}) { {t:Registry} r; {f:EINTR}; }"});
    // Words with digits, a name of one capital (no CamelCase), non-ASCII names.
    CHECK(cpp({"auto x2 = T{}; int città;"}) == Lines{"{t:auto} x2 = T{}; {t:int} città;"});
}

TEST_CASE("syntax: C++ numbers") {
    CHECK(cpp({"a = 42 + 3.14f + 1e-9 + 0x1Fu + 0b1010 + 1'000'000 + .5 + 0x1p-3;"}) ==
          Lines{"a = {n:42} + {n:3.14f} + {n:1e-9} + {n:0x1Fu} + {n:0b1010} + {n:1'000'000} + "
                "{n:.5} + {n:0x1p-3};"});
    // In hexadecimal, e is a digit: 0x1e-3 is 0x1e minus 3.
    CHECK(cpp({"0x1e-3"}) == Lines{"{n:0x1e}-{n:3}"});
}

TEST_CASE("syntax: C++ strings and characters") {
    CHECK(
        cpp({R"(auto s = "a \"quoted\" word"; char c = '\''; char d = 'x';)"}) ==
        Lines{
            R"({t:auto} s = {s:"a \"quoted\" word"}; {t:char} c = {s:'\''}; {t:char} d = {s:'x'};)"});
    CHECK(cpp({R"(f(u8"é", L"w", u'c', U"x");)"}) ==
          Lines{R"({f:f}({s:u8"é"}, {s:L"w"}, {s:u'c'}, {s:U"x"});)"});
    // Not closed: to the end of the line, and the next line is code again.
    CHECK(cpp({"\"open", "int x;"}) == Lines{"{s:\"open}", "{t:int} x;"});
    // An escaped backslash before the closing quote.
    CHECK(cpp({R"("a\\" + b)"}) == Lines{R"({s:"a\\"} + b)"});
}

TEST_CASE("syntax: C++ raw strings span lines") {
    CHECK(cpp({R"~(auto r = R"(no \escape)";)~"}) ==
          Lines{R"~({t:auto} r = {s:R"(no \escape)"};)~"});
    CHECK(
        cpp({R"~(auto r = R"sql(SELECT ")";)~", "FROM t;", R"~()sql" + more;)~"}) ==
        Lines{R"~({t:auto} r = {s:R"sql(SELECT ")";})~", "{s:FROM t;}", R"~({s:)sql"} + more;)~"});
    CHECK(cpp({R"~(x = LR"(w)" u8R"--(a)--";)~"}) ==
          Lines{R"~(x = {s:LR"(w)"} {s:u8R"--(a)--"};)~"});
    // Not a raw string: R alone.
    CHECK(cpp({"R + 1"}) == Lines{"R + {n:1}"});
}

TEST_CASE("syntax: C++ comments span lines") {
    CHECK(cpp({"int x;  // the count"}) == Lines{"{t:int} x;  {c:// the count}"});
    CHECK(cpp({"a /* one */ b"}) == Lines{"a {c:/* one */} b"});
    CHECK(cpp({"/* first", "   second", "   third */ int x;", "int y;"}) ==
          Lines{"{c:/* first}", "{c:   second}", "{c:   third */} {t:int} x;", "{t:int} y;"});
    // Inside a string, comment markers are text; inside a comment, quotes are.
    CHECK(cpp({R"(s = "// not a comment";)"}) == Lines{R"(s = {s:"// not a comment"};)"});
    CHECK(cpp({R"(// it's "fine")", "int x;"}) == Lines{R"({c:// it's "fine"})", "{t:int} x;"});
    CHECK(cpp({"/**/ x"}) == Lines{"{c:/**/} x"});
}

TEST_CASE("syntax: C preprocessor") {
    CHECK(cpp({"#include <vector>"}) == Lines{"{f:#include} {s:<vector>}"});
    CHECK(cpp({"#include \"nordend/text/text.hpp\""}) ==
          Lines{"{f:#include} {s:\"nordend/text/text.hpp\"}"});
    CHECK(cpp({"  #  define MAX(a, b) ((a) > (b) ? (a) : (b))"}) ==
          Lines{"  {f:#  define} {f:MAX}(a, b) ((a) > (b) ? (a) : (b))"});
    CHECK(cpp({"#pragma once"}) == Lines{"{f:#pragma} once"});
    CHECK(cpp({"#if defined(__linux__) // linux"}) ==
          Lines{"{f:#if} {f:defined}(__linux__) {c:// linux}"});
    // Not at the start of a line: just a character.
    CHECK(cpp({"a # b"}) == Lines{"a # b"});
}

TEST_CASE("syntax: lines that are not code still colour safely") {
    // Bytes that are no C++: nothing crashes, every piece is inside its line.
    const Lines odd{"",
                    "'",
                    "\"",
                    "R\"",
                    "R\"(",
                    "/*",
                    "*/",
                    "0x",
                    "1e+",
                    "#",
                    "#include <",
                    "u8",
                    "L'",
                    "\\",
                    "'''",
                    "1'",
                    "R\"abcdefghijklmnopqrstuvwxyz(x)\""};
    const Lines out = cpp(odd);
    CHECK(out.size() == odd.size());
}

TEST_CASE("syntax: the language by extension and interpreter") {
    const auto name_of = [](std::string_view name, std::string_view mime,
                            std::string_view first = {}) -> std::string {
        const family::Language* language = family::language_for(name, mime, first);
        return language == nullptr ? "" : std::string{family::language_name(*language)};
    };
    CHECK(name_of("main.rs", "text/rust") == "Rust");
    CHECK(name_of("a.py", "text/x-python") == "Python");
    CHECK(name_of("a.py", "text/x-python3") == "Python");
    CHECK(name_of("a.js", "text/javascript") == "JavaScript");
    CHECK(name_of("a.lua", "text/x-lua") == "Lua");
    // Types the MIME database gets wrong or does not know: by the extension.
    CHECK(name_of("app.ts", "text/vnd.trolltech.linguist", "import x from 'y';") == "TypeScript");
    CHECK(name_of("app.ts", "text/vnd.trolltech.linguist", "<?xml version=\"1.0\"?>").empty());
    CHECK(name_of("view.tsx", "application/x-tiled-tsx", "export {};") == "TypeScript");
    CHECK(name_of("view.jsx", "text/plain") == "JavaScript");
    CHECK(name_of("plugin.luau", "text/plain") == "Luau");
    CHECK(name_of(".rs", "text/plain").empty());  // a hidden file, not an extension
    // The program after #!.
    CHECK(name_of("tool", "text/plain", "#!/usr/bin/env python3") == "Python");
    CHECK(name_of("tool", "text/plain", "#!/usr/bin/env -S python3 -u") == "Python");
    CHECK(name_of("tool", "text/plain", "#!/usr/bin/python3.12") == "Python");
    CHECK(name_of("tool", "text/plain", "#! /usr/bin/env node") == "JavaScript");
    CHECK(name_of("tool", "text/plain", "#!/usr/bin/lua5.4") == "Lua");
    CHECK(name_of("tool", "text/plain", "#!/usr/bin/pythonista").empty());
}

namespace {

Lines rust(const Lines& lines) { return colour(lines, "text/rust"); }

Lines python(const Lines& lines) { return colour(lines, "text/x-python3"); }

Lines js(const Lines& lines) { return colour(lines, "text/javascript"); }

Lines ts(const Lines& lines) { return colour(lines, "text/plain", "a.ts"); }

Lines lua(const Lines& lines) { return colour(lines, "text/x-lua"); }

}  // namespace

TEST_CASE("syntax: Rust") {
    CHECK(rust({"fn main() {"}) == Lines{"{k:fn} {f:main}() {"});
    CHECK(rust({"let v: Vec<u32> = vec![1, 2];"}) ==
          Lines{"{k:let} v: {t:Vec}<{t:u32}> = {f:vec!}[{n:1}, {n:2}];"});
    CHECK(rust({"#[derive(Debug, Clone)]"}) == Lines{"{f:#[derive(Debug, Clone)]}"});
    CHECK(rust({"#![allow(dead_code)]"}) == Lines{"{f:#![allow(dead_code)]}"});
    // Lifetimes and characters.
    CHECK(
        rust({"fn f<'a>(s: &'a str) -> Option<char> { Some('x') }"}) ==
        Lines{
            "{k:fn} f<{t:'a}>(s: &{t:'a} {t:str}) -> {t:Option}<{t:char}> { {n:Some}({s:'x'}) }"});
    CHECK(rust({R"(let c = '\''; let d = 'é'; let s: &'static str;)"}) ==
          Lines{R"({k:let} c = {s:'\''}; {k:let} d = {s:'é'}; {k:let} s: &{t:'static} {t:str};)"});
    // Raw, byte and plain strings; plain ones go on over lines.
    CHECK(rust({R"~(let r = r#"a "quoted" b"#;)~"}) ==
          Lines{R"~({k:let} r = {s:r#"a "quoted" b"#};)~"});
    CHECK(rust({R"(let b = b"bytes"; let c = b'a'; let r = br"\d";)"}) ==
          Lines{R"({k:let} b = {s:b"bytes"}; {k:let} c = {s:b'a'}; {k:let} r = {s:br"\d"};)"});
    CHECK(rust({"let s = \"one", "two\";"}) == Lines{"{k:let} s = {s:\"one}", "{s:two\"};"});
    CHECK(rust({R"~(let r = r##"one)~", R"~(two"## + x;)~"}) ==
          Lines{R"~({k:let} r = {s:r##"one})~", R"~({s:two"##} + x;)~"});
    // Comments nest.
    CHECK(rust({"/* a /* b */ c */ x"}) == Lines{"{c:/* a /* b */ c */} x"});
    CHECK(rust({"/* a /* b */", "c */ x"}) == Lines{"{c:/* a /* b */}", "{c:c */} x"});
    // Numbers, ranges, constants in capitals; != is no macro.
    CHECK(rust({"for i in 0..10 { x != 1_000u64 + 2.5e-3; MAX_SIZE }"}) ==
          Lines{"{k:for} i {k:in} {n:0}..{n:10} { x != {n:1_000u64} + {n:2.5e-3}; {n:MAX_SIZE} }"});
}

TEST_CASE("syntax: Python") {
    CHECK(python({"def greet(name: str) -> None:"}) ==
          Lines{"{k:def} {f:greet}(name: {t:str}) -> {n:None}:"});
    CHECK(python({"@app.route(\"/\")"}) == Lines{"{f:@app.route}({s:\"/\"})"});
    CHECK(python({R"(x = f"hi {name}" + r'\d' + b"\x00" + Rb'\w' + u"é")"}) ==
          Lines{R"(x = {s:f"hi {name}"} + {s:r'\d'} + {s:b"\x00"} + {s:Rb'\w'} + {s:u"é"})"});
    // Triple quotes over lines; one quote ends with its line.
    CHECK(python({R"(""")", "Docs, with 'quotes'.", R"(""" + x)"}) ==
          Lines{R"({s:"""})", R"({s:Docs, with 'quotes'.})", R"({s:"""} + x)"});
    CHECK(python({"s = 'open", "x = 1"}) == Lines{"s = {s:'open}", "x = {n:1}"});
    CHECK(python({"class Point(Base):  # a comment"}) ==
          Lines{"{k:class} {f:Point}({t:Base}):  {c:# a comment}"});
    CHECK(python({"self.ready = True; MAX = 10"}) ==
          Lines{"{k:self}.ready = {n:True}; {n:MAX} = {n:10}"});
    // A string before a word glued to it: not a prefix.
    CHECK(python({"print(name\"x\")"}) == Lines{"{f:print}(name{s:\"x\"})"});
}

TEST_CASE("syntax: JavaScript and TypeScript") {
    CHECK(js({"const x = `a ${b}`; let y = null ?? undefined;"}) ==
          Lines{"{k:const} x = {s:`a ${b}`}; {k:let} y = {n:null} ?? {n:undefined};"});
    CHECK(js({"const t = `one", "two`;"}) == Lines{"{k:const} t = {s:`one}", "{s:two`};"});
    CHECK(js({"/** docs */ export default function f() {}"}) ==
          Lines{"{c:/** docs */} {k:export} {k:default} {k:function} {f:f}() {}"});
    CHECK(ts({"function f(a: number): string {"}) ==
          Lines{"{k:function} {f:f}(a: {t:number}): {t:string} {"});
    CHECK(ts({"@Component({}) class App implements OnInit {}"}) ==
          Lines{"{f:@Component}({}) {k:class} {t:App} {k:implements} {t:OnInit} {}"});
}

TEST_CASE("syntax: Lua and Luau") {
    CHECK(lua({"local function f(x) return x .. 'y' end"}) ==
          Lines{"{k:local} {k:function} {f:f}(x) {k:return} x .. {s:'y'} {k:end}"});
    CHECK(lua({"x = nil -- a comment"}) == Lines{"x = {n:nil} {c:-- a comment}"});
    CHECK(lua({"--[[ long", "comment ]] x = 1"}) ==
          Lines{"{c:--[[ long}", "{c:comment ]]} x = {n:1}"});
    CHECK(lua({"--[==[ a ]] b ]==] y"}) == Lines{"{c:--[==[ a ]] b ]==]} y"});
    CHECK(lua({"s = [[raw", "text]] .. t[1]"}) == Lines{"s = {s:[[raw}", "{s:text]]} .. t[{n:1}]"});
    CHECK(lua({"s = [=[a]]b]=]"}) == Lines{"s = {s:[=[a]]b]=]}"});
    CHECK(colour({"local n: number = 1"}, "text/plain", "x.luau") ==
          Lines{"{k:local} n: {t:number} = {n:1}"});
}
