#include "nordend/formats/text/syntax.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "languages.hpp"
#include "nordend/text/text.hpp"

namespace nordend::formats::text {
namespace {

using nordend::text::Span;
using nordend::text::Style;

// Where a line ends inside something that goes on to the next.
struct State {
    enum class In : std::uint8_t { code, comment, string };
    In in{In::code};
    std::string close;    // what ends it: "*/", ")delim\"", "\"\"\"", "]==]"
    bool escapes{false};  // a string: a backslash escapes the next character
    int depth{0};         // nested comments: how many are open
};

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }

bool is_lower(char c) { return c >= 'a' && c <= 'z'; }

bool is_word_start(char c) {
    return is_upper(c) || is_lower(c) || c == '_' || static_cast<unsigned char>(c) >= 0x80U;
}

bool is_word(char c) { return is_word_start(c) || is_digit(c); }

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v'; }

char lower(char c) { return is_upper(c) ? static_cast<char>(c - 'A' + 'a') : c; }

// What the lexer looks up, made once for each syntax.
struct Tables {
    std::array<bool, 256> opens{};   // may start a comment, a string, a number or a word
    std::array<bool, 256> quotes{};  // opens a string
    std::unordered_map<std::string_view, Style> words;  // keywords, constants and types
};

Tables make_tables(const Syntax& syntax) {
    Tables tables;
    const auto open = [&](char c) { tables.opens.at(static_cast<unsigned char>(c)) = true; };
    for (const std::string_view marker : syntax.line_comments) {
        open(marker.front());
    }
    if (!syntax.block_open.empty()) {
        open(syntax.block_open.front());
    }
    for (const char quote : syntax.quotes) {
        open(quote);
        tables.quotes.at(static_cast<unsigned char>(quote)) = true;
    }
    if (syntax.lua_long_brackets) {
        open('[');
    }
    if (syntax.decorator != '\0') {
        open(syntax.decorator);
    }
    if (syntax.rust_attributes) {
        open('#');
    }
    for (int c = 0; c < 256; ++c) {
        const auto byte = static_cast<char>(c);
        if (is_digit(byte) || is_word_start(byte) || byte == '.') {
            open(byte);
        }
    }
    // Later lists win: a keyword over a constant over a type.
    for (const auto& [list, style] :
         {std::pair{syntax.types, Style::type}, std::pair{syntax.constants, Style::number},
          std::pair{syntax.keywords, Style::keyword}}) {
        for (const std::string_view word : list) {
            tables.words[word] = style;
        }
    }
    return tables;
}

// Every syntax's tables, made the first time any is needed.
const Tables& tables_of(const Syntax& syntax) {
    static const std::unordered_map<const Syntax*, Tables> kTables = [] {
        std::unordered_map<const Syntax*, Tables> all;
        for (const Language& language : all_languages()) {
            if (!all.contains(language.syntax)) {
                all.emplace(language.syntax, make_tables(*language.syntax));
            }
        }
        return all;
    }();
    return kTables.at(&syntax);
}

// The length of a Lua long bracket at the start of text: "[[" 2, "[==[" 4; 0 when none.
std::size_t long_bracket(std::string_view text) {
    if (!text.starts_with('[')) {
        return 0;
    }
    std::size_t i = 1;
    while (i < text.size() && text[i] == '=') {
        ++i;
    }
    return i < text.size() && text[i] == '[' ? i + 1 : 0;
}

class LineLexer {
public:
    LineLexer(const Syntax& syntax, const Tables& tables, std::string_view line, State& state,
              std::vector<Span>& out)
        : syntax_{syntax}, tables_{tables}, line_{line}, state_{state}, out_{out} {}

    void run() {
        if (state_.in == State::In::comment) {
            go_on_comment(0);
        } else if (state_.in == State::In::string) {
            go_on_string(0);
        }
        if (state_.in == State::In::code && pos_ == 0 && syntax_.preprocessor) {
            directive();
        }
        while (pos_ < line_.size() && state_.in == State::In::code) {
            step();
        }
    }

private:
    void emit(std::size_t start, std::size_t end, Style style) {
        if (end > start) {
            out_.push_back({.start = static_cast<std::uint32_t>(start),
                            .length = static_cast<std::uint32_t>(end - start),
                            .style = style});
        }
    }

    [[nodiscard]] bool at(std::string_view text) const {
        return !text.empty() && line_.substr(pos_).starts_with(text);
    }

    [[nodiscard]] char next(std::size_t ahead = 1) const {
        return pos_ + ahead < line_.size() ? line_[pos_ + ahead] : '\0';
    }

    // A comment from start, its opening already passed, to state_.close (counting nested
    // openings when the syntax nests them); to the end of the line when it goes on.
    void go_on_comment(std::size_t start) {
        if (!syntax_.nested_comments) {
            const std::size_t end = line_.find(state_.close, pos_);
            if (end != std::string_view::npos) {
                pos_ = end + state_.close.size();
                emit(start, pos_, Style::comment);
                state_ = {};
                return;
            }
            pos_ = line_.size();
        }
        while (pos_ < line_.size()) {
            if (at(syntax_.block_open)) {
                ++state_.depth;
                pos_ += syntax_.block_open.size();
            } else if (at(state_.close)) {
                pos_ += state_.close.size();
                if (--state_.depth <= 0) {
                    emit(start, pos_, Style::comment);
                    state_ = {};
                    return;
                }
            } else {
                ++pos_;
            }
        }
        emit(start, line_.size(), Style::comment);
        state_.in = State::In::comment;
    }

    void open_comment(std::size_t start, std::size_t opening, std::string close) {
        pos_ = start + opening;
        state_ = {
            .in = State::In::comment, .close = std::move(close), .escapes = false, .depth = 1};
        go_on_comment(start);
    }

    // A string from start, its opening already passed, to state_.close; to the end of the line
    // when it goes on (for strings that may: otherwise it ends there).
    void go_on_string(std::size_t start) {
        const std::string_view close = state_.close;
        std::size_t end = std::string_view::npos;
        if (!state_.escapes) {
            end = line_.find(close, pos_);
        } else {
            // Most strings close with one quote: compare characters, not substrings.
            for (std::size_t i = pos_; i < line_.size(); ++i) {
                if (line_[i] == '\\') {
                    ++i;
                } else if (line_[i] == close.front() && line_.substr(i).starts_with(close)) {
                    end = i;
                    break;
                }
            }
        }
        if (end == std::string_view::npos) {
            pos_ = line_.size();
            emit(start, pos_, Style::string);
            return;
        }
        pos_ = end + close.size();
        emit(start, pos_, Style::string);
        state_ = {};
    }

    // A string opened at start, the text before its quote at quote a prefix; over lines when
    // multiline (a backslash at the end of a line goes on too, in any string with escapes).
    void open_string(std::size_t start, std::size_t quote, std::string close, bool escapes,
                     bool multiline) {
        pos_ = quote + close.size();
        state_ = {
            .in = State::In::string, .close = std::move(close), .escapes = escapes, .depth = 0};
        go_on_string(start);
        if (state_.in == State::In::string && !multiline && !(escapes && line_.ends_with('\\'))) {
            state_ = {};  // a string never closed: it ends with its line
        }
    }

    // # include <file>: the directive as a macro, an included file as a string.
    void directive() {
        std::size_t i = 0;
        while (i < line_.size() && is_space(line_[i])) {
            ++i;
        }
        if (i >= line_.size() || line_[i] != '#') {
            return;
        }
        const std::size_t hash = i++;
        while (i < line_.size() && is_space(line_[i])) {
            ++i;
        }
        const std::size_t word = i;
        while (i < line_.size() && is_word(line_[i])) {
            ++i;
        }
        emit(hash, i, Style::function);
        const std::string_view name = line_.substr(word, i - word);
        pos_ = i;
        if (name == "include" || name == "include_next" || name == "import" || name == "embed") {
            while (pos_ < line_.size() && is_space(line_[pos_])) {
                ++pos_;
            }
            if (pos_ < line_.size() && line_[pos_] == '<') {
                const std::size_t close = line_.find('>', pos_);
                const std::size_t end = close == std::string_view::npos ? line_.size() : close + 1;
                emit(pos_, end, Style::string);
                pos_ = end;
            }
        }
    }

    void step() {
        const char c = line_[pos_];
        if (!tables_.opens.at(static_cast<unsigned char>(c))) {
            ++pos_;  // a space, an operator, punctuation
            return;
        }
        // Words first, the most common: no comment marker starts with a letter, and prefixed
        // and raw strings start as words.
        if (is_word_start(c)) {
            word();
            return;
        }
        if (syntax_.lua_long_brackets) {
            if (at("--")) {
                if (const std::size_t length = long_bracket(line_.substr(pos_ + 2))) {
                    open_comment(pos_, 2 + length, "]" + std::string(length - 2, '=') + "]");
                    return;
                }
            } else if (const std::size_t length = long_bracket(line_.substr(pos_))) {
                // [==[ ends at ]==]: as many "=" between the brackets.
                open_string(pos_, pos_, "]" + std::string(length - 2, '=') + "]", false, true);
                return;
            }
        }
        for (const std::string_view marker : syntax_.line_comments) {
            if (at(marker)) {
                emit(pos_, line_.size(), Style::comment);
                pos_ = line_.size();
                return;
            }
        }
        if (at(syntax_.block_open)) {
            open_comment(pos_, syntax_.block_open.size(), std::string{syntax_.block_close});
            return;
        }
        if (syntax_.rust_attributes && c == '#' &&
            (next() == '[' || (next() == '!' && next(2) == '['))) {
            attribute();
            return;
        }
        if (c == syntax_.decorator && is_word_start(next())) {
            decorator();
            return;
        }
        if (syntax_.lifetimes && c == '\'' && lifetime()) {
            return;
        }
        if (tables_.quotes.at(static_cast<unsigned char>(c))) {
            quoted(pos_, pos_, false);
            return;
        }
        // .5 is a number; the 10 of 0..10 starts at its digit.
        if (is_digit(c) ||
            (c == '.' && is_digit(next()) && (pos_ == 0 || line_[pos_ - 1] != '.'))) {
            number();
            return;
        }
        ++pos_;
    }

    // A string opened by the quote at quote, its prefix from start; raw: no escapes.
    void quoted(std::size_t start, std::size_t quote, bool raw) {
        const char q = line_[quote];
        if (syntax_.triple_quotes && line_.substr(quote).starts_with(std::string(3, q))) {
            open_string(start, quote, std::string(3, q), !raw, true);
            return;
        }
        const bool multiline = syntax_.multiline_quotes.find(q) != std::string_view::npos;
        open_string(start, quote, std::string(1, q), !raw, multiline);
    }

    // R"delimiter( ... )delimiter" (C++): the prefix starts at start, the quote is at quote.
    bool cpp_raw(std::size_t start, std::size_t quote) {
        const std::size_t open = line_.find('(', quote + 1);
        if (open == std::string_view::npos || open - quote - 1 > 16) {
            return false;
        }
        const std::string_view delimiter = line_.substr(quote + 1, open - quote - 1);
        if (std::ranges::any_of(delimiter, [](char d) {
                return is_space(d) || d == '\\' || d == ')' || d == '"';
            })) {
            return false;
        }
        pos_ = open + 1;
        state_ = {.in = State::In::string,
                  .close = ")" + std::string{delimiter} + "\"",
                  .escapes = false,
                  .depth = 0};
        go_on_string(start);
        return true;
    }

    // r"...", r#"..."# (Rust): the prefix starts at start, the hashes or the quote at pos_.
    bool rust_raw(std::size_t start) {
        std::size_t i = pos_;
        while (i < line_.size() && line_[i] == '#') {
            ++i;
        }
        if (i >= line_.size() || line_[i] != '"') {
            return false;
        }
        const std::size_t hashes = i - pos_;
        pos_ = i + 1;
        state_ = {.in = State::In::string,
                  .close = "\"" + std::string(hashes, '#'),
                  .escapes = false,
                  .depth = 0};
        go_on_string(start);
        return true;
    }

    // #[attribute] and #![attribute], brackets inside counted, on one line.
    void attribute() {
        const std::size_t start = pos_;
        int depth = 0;
        while (pos_ < line_.size()) {
            const char c = line_[pos_++];
            if (c == '[') {
                ++depth;
            } else if (c == ']' && --depth == 0) {
                break;
            }
        }
        emit(start, pos_, Style::function);
    }

    // @name, @module.name
    void decorator() {
        const std::size_t start = pos_++;
        while (pos_ < line_.size() && (is_word(line_[pos_]) || line_[pos_] == '.')) {
            ++pos_;
        }
        emit(start, pos_, Style::function);
    }

    // 'a' and '\n' are characters; 'a and 'static lifetimes (Rust). False: neither, the quote
    // is left for strings.
    bool lifetime() {
        if (next() == '\\') {
            return false;  // an escaped character: a string
        }
        // One character (of one to four bytes) and a closing quote: a character.
        std::size_t after = pos_ + 2;
        while (after < line_.size() &&
               (static_cast<unsigned char>(line_[after]) & 0xC0U) == 0x80U) {
            ++after;
        }
        if (after < line_.size() && line_[after] == '\'') {
            return false;
        }
        if (!is_word_start(next())) {
            return false;
        }
        const std::size_t start = pos_++;
        while (pos_ < line_.size() && is_word(line_[pos_])) {
            ++pos_;
        }
        emit(start, pos_, Style::type);
        return true;
    }

    void number() {
        const std::size_t start = pos_;
        const bool hex =
            line_.substr(start).starts_with("0x") || line_.substr(start).starts_with("0X");
        while (pos_ < line_.size()) {
            const char c = line_[pos_];
            const char before = pos_ > start ? line_[pos_ - 1] : '\0';
            const bool exponent_sign =
                (c == '+' || c == '-') &&
                (((before == 'e' || before == 'E') && !hex) || before == 'p' || before == 'P');
            const bool separator = c == '\'' && syntax_.digit_separators &&
                                   pos_ + 1 < line_.size() && is_word(line_[pos_ + 1]);
            // A range, 0..10 (Rust, Lua's concatenation): the number ends before it.
            const bool range = c == '.' && next() == '.';
            if (range || (!is_word(c) && c != '.' && !exponent_sign && !separator)) {
                break;
            }
            ++pos_;
        }
        emit(start, pos_, Style::number);
    }

    // Whether name, glued to a quote, is one of the syntax's string prefixes; raw when it
    // holds the raw letter.
    [[nodiscard]] bool string_prefix(std::string_view name, bool& raw) const {
        std::string folded{name};
        if (syntax_.prefix_any_case) {
            std::ranges::transform(folded, folded.begin(), lower);
        }
        raw = syntax_.raw_letter != '\0' && folded.find(syntax_.raw_letter) != std::string::npos;
        return std::ranges::find(syntax_.string_prefixes, folded) != syntax_.string_prefixes.end();
    }

    void word() {
        const std::size_t start = pos_;
        while (pos_ < line_.size() && is_word(line_[pos_])) {
            ++pos_;
        }
        const std::string_view name = line_.substr(start, pos_ - start);
        const char after = pos_ < line_.size() ? line_[pos_] : '\0';
        if (syntax_.raw_strings == RawStrings::cpp && after == '"' &&
            (name == "R" || name == "u8R" || name == "uR" || name == "UR" || name == "LR") &&
            cpp_raw(start, pos_)) {
            return;
        }
        if (syntax_.raw_strings == RawStrings::rust && (after == '"' || after == '#') &&
            (name == "r" || name == "br" || name == "cr") && rust_raw(start)) {
            return;
        }
        bool raw = false;
        if (tables_.quotes.at(static_cast<unsigned char>(after)) && string_prefix(name, raw)) {
            quoted(start, pos_, raw);
            return;
        }
        if (syntax_.bang_macros && after == '!' && next() != '=') {
            ++pos_;
            emit(start, pos_, Style::function);
            return;
        }
        if (const std::optional<Style> style = classify(name)) {
            emit(start, pos_, *style);
        }
    }

    // What a word is; nullopt for a plain name.
    [[nodiscard]] std::optional<Style> classify(std::string_view name) const {
        if (const auto found = tables_.words.find(name); found != tables_.words.end()) {
            return found->second;
        }
        if (syntax_.suffix_t_types && name.size() > 2 && name.ends_with("_t")) {
            return Style::type;
        }
        if (syntax_.capitals && std::ranges::none_of(name, is_lower) &&
            std::ranges::count_if(name, is_upper) >= 2) {
            return syntax_.capitals;
        }
        // Called: the next thing after spaces is "(".
        std::size_t after = pos_;
        while (after < line_.size() && is_space(line_[after])) {
            ++after;
        }
        if (after < line_.size() && line_[after] == '(') {
            return Style::function;
        }
        if (syntax_.camel_case_types && is_upper(name.front()) &&
            std::ranges::any_of(name, is_lower)) {
            return Style::type;
        }
        return std::nullopt;
    }

    const Syntax& syntax_;
    const Tables& tables_;
    std::string_view line_;
    State& state_;
    std::vector<Span>& out_;
    std::size_t pos_{0};
};

// The program a #! line runs: #!/usr/bin/env -S python3 -u gives "python3".
std::string_view interpreter_of(std::string_view first_line) {
    if (!first_line.starts_with("#!")) {
        return {};
    }
    std::string_view command = first_line.substr(2);
    const auto next_word = [&] {
        const std::size_t begin = command.find_first_not_of(" \t");
        if (begin == std::string_view::npos) {
            return std::string_view{};
        }
        const std::size_t end = command.find_first_of(" \t", begin);
        const std::string_view word = command.substr(begin, end - begin);
        command = end == std::string_view::npos ? std::string_view{} : command.substr(end);
        return word;
    };
    std::string_view program = next_word();
    program = program.substr(program.rfind('/') + 1);
    if (program == "env") {
        program = next_word();
        while (program.starts_with('-')) {
            program = next_word();
        }
    }
    return program;
}

}  // namespace

const Language* language_for(std::string_view name, std::string_view mime,
                             std::string_view first_line) {
    const std::span<const Language> languages = all_languages();
    for (const Language& language : languages) {
        if (std::ranges::find(language.mime, mime) != language.mime.end()) {
            return &language;
        }
    }
    for (const Language& language : languages) {
        if (std::ranges::find(language.file_names, name) != language.file_names.end()) {
            return &language;
        }
    }
    // By the extension, unless the file looks like markup: a Qt Linguist .ts is XML.
    const std::size_t dot = name.rfind('.');
    if (dot != std::string_view::npos && dot > 0 && !first_line.starts_with('<')) {
        const std::string_view extension = name.substr(dot);
        for (const Language& language : languages) {
            if (std::ranges::find(language.extensions, extension) != language.extensions.end()) {
                return &language;
            }
        }
    }
    // The program its first line runs, its version allowed after the name: python3.12.
    const std::string_view program = interpreter_of(first_line);
    if (!program.empty()) {
        for (const Language& language : languages) {
            for (const std::string_view interpreter : language.interpreters) {
                if (program.starts_with(interpreter) &&
                    program.substr(interpreter.size()).find_first_not_of("0123456789.") ==
                        std::string_view::npos) {
                    return &language;
                }
            }
        }
    }
    return nullptr;
}

std::string_view language_name(const Language& language) { return language.name; }

void highlight(nordend::text::Text& text, const Language& language) {
    text.language = language.name;
    text.spans.assign(text.lines.size(), {});
    const Tables& tables = tables_of(*language.syntax);
    State state;
    for (std::size_t i = 0; i < text.lines.size(); ++i) {
        LineLexer{*language.syntax, tables, text.lines[i], state, text.spans[i]}.run();
    }
}

void colour(nordend::text::Text& text, std::string_view name, std::string_view mime) {
    const Language* language =
        language_for(name, mime, text.lines.empty() ? std::string_view{} : text.lines.front());
    if (language != nullptr) {
        highlight(text, *language);
    }
}

}  // namespace nordend::formats::text
