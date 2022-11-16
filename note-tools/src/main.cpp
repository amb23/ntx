#include <ctime>
#include <deque>
#include <exception>
#include <iostream>
#include <fstream>
#include <regex>
#include <optional>
#include <string>
#include <sstream>
#include <variant>
#include <vector>

#include <boost/program_options.hpp>

#include "version.hpp"


namespace ntx {

struct Exception : public std::exception
{
    Exception() : std::exception{}, m_message{} {}
    
    template <typename... Ts>
    explicit Exception(Ts... args)
        : std::exception{}
        , m_message{
            std::invoke(
                [&]() -> std::string
                {
                    std::stringstream s;
                    (s << ... << args);
                    return s.str();
                }
            )
        }
    { }

    std::string m_message;
};


namespace patterns {

static const std::regex tag{"^% TAG (.+)"};
static const std::regex cmd{"^% CMD (.+) (.+)"};
static const std::regex env_decl{"^[ ]*\\\\([A-z]+)[ ]*([^ ]*)"};
static const std::regex section_def{"^[ ]*\\\\([A-z]+)[ ]*(.*)"};

} // patterns


struct LineBreak
{
    std::size_t m_line_number;

    bool is_empty;
    std::size_t m_indent;
};


enum class EnvironmentType
{
    Section,
    PlainText,
    MathBlock,
    MathInline,
    Array,
    ListItem
};

std::ostream& operator << (std::ostream& os, EnvironmentType e_type) {
    switch (e_type) {
    case EnvironmentType::Section:
        os << "Section";
        break;
    case EnvironmentType::PlainText:
        os << "PlainText";
        break;
    case EnvironmentType::MathBlock:
        os << "MathBlock";
        break;
    case EnvironmentType::MathInline:
        os << "MathInline";
        break;
    case EnvironmentType::Array:
        os << "Array";
        break;
    case EnvironmentType::ListItem:
        os << "ListItem";
        break;
    default:
        assert(false);
    }

    return os;
}

struct EnvironmentDecleration
{
    std::size_t m_line_number;

    EnvironmentType m_type;
    std::string m_name;
    std::string m_label;
};

struct Element
{
    std::size_t m_line_number;
    std::size_t m_line_pos;

    //std::size_t m_sentence_pos;

    // These could all be smaller
    std::size_t m_open_b_round;
    std::size_t m_open_b_square;
    std::size_t m_open_b_curly;

    std::size_t n_lower_case;
    std::size_t n_upper_case;
    std::size_t n_numerals;

    std::string m_data;
};


using element_v = std::variant<LineBreak, EnvironmentDecleration, Element>;


std::size_t get_line_number(const element_v& element) {
    return std::visit([](auto&& a) { return a.m_line_number; }, element);
}


using env_data_t = std::tuple<EnvironmentType, std::string>;

static const std::unordered_map<std::string, env_data_t> s_environments = {
    {"proof",         {EnvironmentType::PlainText, "proof"}},
    {"thm",           {EnvironmentType::PlainText, "theorem"}},
    {"theorem",       {EnvironmentType::PlainText, "theorem"}},
    {"lemma",         {EnvironmentType::PlainText, "lemma"}},
    {"corollary",     {EnvironmentType::PlainText, "corollary"}},
    {"prop",          {EnvironmentType::PlainText, "proposition"}},
    {"proposition",   {EnvironmentType::PlainText, "proposition"}},
    {"construction",  {EnvironmentType::PlainText, "construction"}},
    {"eq",            {EnvironmentType::MathBlock, "equation"}},
    {"equation",      {EnvironmentType::MathBlock, "equation"}},
    {"section",       {EnvironmentType::Section, "section"}},
    {"subsection",    {EnvironmentType::Section,"subsection"}},
    {"subsubsection", {EnvironmentType::Section,"subsubsection"}},
};


struct BlockElement
{
    std::string m_data;
};

struct Block
{
    EnvironmentType m_type;

    std::size_t m_indent;

    std::size_t m_initial_pos;

    std::string m_name;
    std::string m_label;

    // FIXME - can we get away with just the initial pos here and only write
    // the data on completion (or maybe srting_views rather than strings...)
    std::vector<BlockElement> m_data = {};
};

template <typename T, typename... Ts>
bool contains_any_of(const std::string& s, T c, Ts... cs) {
    bool out = s.find(c) != std::string::npos;
    if constexpr (sizeof...(Ts) > 0) {
        return out || contains_any_of(s, cs...);
    }
    else {
        return out;
    }
}

template <typename T, typename... Ts>
bool starts_with_any_of(const std::string& s, T t, Ts... ts) {
    if constexpr (sizeof...(Ts) > 0) {
        return s.starts_with(t) || starts_with_any_of(s, ts...);
    }
    else {
        return s.starts_with(t);
    }
}


template <typename T, typename... Ts>
bool ends_with_any_of(const std::string& s, T t, Ts... ts) {
    if constexpr (sizeof...(Ts) > 0) {
        return s.ends_with(t) || ends_with_any_of(s, ts...);
    }
    else {
        return s.ends_with(t);
    }
}


std::string get_clean_view(
    const std::string& word,
    const BlockElement* next) {
    if (!next) {
        // last element just return it
        return word;
    }

    const auto& w_next = next->m_data;

    if (ends_with_any_of(word, '{', '[', '(', '"', '\'', '\n') ||
        starts_with_any_of(w_next, '}', ']', ')', '.', ',', ';', ':', '"', '\'') ||
        (word.starts_with('\\') && starts_with_any_of(w_next, '{', '[', '('))){
        return word;
    }

    return word + ' ';
}

std::vector<std::string> amalgamate_block(const Block& block) {
    std::vector<std::string> amalgamates = {std::string{}};
    // Common views
    auto clean_view_f = [&](const auto& data_raw, std::size_t n_data) {
            // Drop all terminal new lines (we will add them back)
            for (auto it = data_raw.rbegin(); it != data_raw.rend() && it->m_data == "\n"; ++it) {
                --n_data;
            }

            for (std::size_t pos = 0; pos < n_data; ++pos) {
                amalgamates.front() += get_clean_view(
                    data_raw[pos].m_data,
                    pos + 1 < n_data ? &data_raw[pos + 1] : nullptr);
            }

            for (; n_data < data_raw.size(); ++n_data) {
                amalgamates.emplace_back("\n");
            }
    };
    
    const auto& data = block.m_data;

    switch (block.m_type)
    {
    case EnvironmentType::PlainText:
        {
            if (block.m_name.length()) {
                amalgamates.front() += "\n\\begin{" + block.m_name + "}\n";
                if (block.m_label.length()) {
                    amalgamates.front() += "\\label{" + block.m_label + "}\n";
                }
            }

            clean_view_f(data, data.size());

            if (block.m_name.length()) {
                amalgamates.front() += "\n\\end{" + block.m_name + "}\n";
            }
            break;
        }
    case EnvironmentType::MathBlock:
        {
            bool has_label = block.m_label.length();
            std::string name = has_label ? "equation" : "equation*";
            amalgamates.front() += "\n\\begin{" + name + "}\n";
            if (has_label) {
                amalgamates.front() += "\\label{" + block.m_label + "}\n";
            }

            clean_view_f(data, data.size());

            amalgamates.front() += "\n\\end{" + name + "}\n";
            break;
        }
    case EnvironmentType::MathInline:
        {
            amalgamates.front() += "$";
            assert (!data.empty());
            if (const auto& s = data.back().m_data; ends_with_any_of(s, ';', ',', '.', ':')) {
                clean_view_f(data, data.size() - 1);
                amalgamates.front() += s.substr(0, s.length() - 1) + "$";
                amalgamates.insert(amalgamates.begin() + 1, std::string{s[s.length() - 1]});
            }
            else {
                clean_view_f(data, data.size());
                amalgamates.front() += "$";
            }
            break;
        }
    case EnvironmentType::Array:
        {
            std::size_t max_row_length = 0;
            std::size_t current_row_length = 0;
            
            for (const auto& e : block.m_data) {
                if (e.m_data.ends_with(';')) {
                    max_row_length = std::max(max_row_length, current_row_length + (e.m_data.length() > 1));
                    current_row_length = 0;
                }
                else {
                    ++current_row_length;
                }
            }

            max_row_length = std::max(max_row_length, current_row_length);
            current_row_length = 0;

            amalgamates.front() += "\\left\\( \\begin{array}{" + std::string(max_row_length, 'c') + "} ";
            for (const auto& e : block.m_data) {
                if (e.m_data.ends_with(';')) {
                    if (auto n_e = e.m_data.length(); n_e > 1) {
                        amalgamates.front() += e.m_data.substr(0, n_e - 1) + " ";
                    }
                    amalgamates.front() += "// ";
                    current_row_length = 0;
                }
                else {
                    amalgamates.front() += e.m_data + (++current_row_length < max_row_length ? " & " : " ");
                }
            }
            amalgamates.front() += "\\end{array} \\right\\)";
            break;
        }
    case EnvironmentType::ListItem:
        {
            if (block.m_label == "first") {
                amalgamates.back() += "\n\\begin{enumerate}";
            }
            amalgamates.back() += '\n' + block.m_name + ' ';
            clean_view_f(data, data.size());
            if (block.m_label == "first") {
                amalgamates.back() += "\n\\end{enumerate}";
            }
            break;
        }
    default:
        // Section
        assert(false);
    }
    return amalgamates;
}

void push_last_block(std::deque<Block>& history) {
    // FIXME - check that all brackets are closed...
    auto& block = history.back();
    auto amalgamated = amalgamate_block(block);
    history.pop_back();
    std::transform(
        amalgamated.begin(),
        amalgamated.end(),
        std::back_inserter(history.back().m_data),
        [](auto&& a) { return BlockElement{a}; }
    );
}

using item_label_t = std::tuple<std::string, std::size_t /* end of label */>;

std::optional<item_label_t> try_get_item_label(
    std::size_t pos,
    const std::vector<element_v>& elements) {
    if (pos == 0 || !std::holds_alternative<LineBreak>(elements[pos - 1])) {
        return std::nullopt;
    }

    //    * ...
    if (std::holds_alternative<Element>(elements[pos]) &&
        std::get<Element>(elements[pos]).m_data == "*") {
        return item_label_t{"\\item", pos + 1};
    }

    // [[ {some labelling} ]] ...
    {
        std::size_t stage = 0;
        bool matched = false;
        std::string label = "";
        std::size_t offset = 0;
        for (; pos + offset < elements.size(); ++offset) {
            if (!std::holds_alternative<Element>(elements[pos + offset])) {
                goto escape_loop;
            }
            const auto& s = std::get<Element>(elements[pos + offset]).m_data;
            switch (stage)
            {
            case 0:
            case 1:
                {
                    if (s == "[") { ++stage; }
                    else { goto escape_loop; }
                    break;
                }
            case 2:
                if (s == "]") { ++stage; }
                else { label += (label.length() ? " " : "") + s; }
                break;
            case 3:
                {
                    if (s == "]") { matched = true; }
                    else { goto escape_loop; }
                    break;
                }
            }
        }
        escape_loop:
        if (matched) {
            // FIXME - what if the label needs to be in mathmode...
            return item_label_t{"\\item[" + label + "]", pos + offset + 1};
        }
    }

    return std::nullopt;
}


bool cannot_take(const std::string& s) {
    return s.find_first_of("}])") != std::string::npos;
}


std::size_t take_at_least_one(const std::vector<BlockElement>& history) {
    using namespace std;

    size_t n_history = history.size();
    if (!n_history || cannot_take(history[n_history - 1].m_data)) return 0;

    // TODO - do I ever need to take more?
    return 1;
}


std::optional<std::size_t> start_math_inline(
    const Element& element,
    const std::vector<BlockElement>& history) {

    using namespace std;
    const auto& s = element.m_data;
    switch (s.length())
    {
    case 0:
        return std::nullopt;
    case 1:
        {
            char c = s[0];
            if (element.n_numerals) return nullopt;
            if (element.n_lower_case) return c == 'a' ? nullopt : optional<size_t>{0};
            if (element.n_upper_case) return c == 'I' ? nullopt : optional<size_t>{0};
            if (c =='[' || c == '{') return 0;
            if (c == '=' || c == '-' || c == '+' || c =='/' || c =='*' || c == '^' || c == '_' ||
                c == '<' || c == '>') {
                return take_at_least_one(history);
            }
            return nullopt;
        }
    case 2:
        {
            // FIXME - do we want to check if closing
            if (element.n_lower_case == 0) {
                return 0;
            }
            else if (element.n_lower_case == 1) {
                if (element.n_upper_case) return s[0] > s[1] ? optional<size_t>{0} : nullopt;
                return 0;
            }
            return nullopt;
        }
    default:
        {
            if (cannot_take(s)) {
                return nullopt;
            }
            if (s.find_first_of("\\[{^_") != string::npos) {
                // TODO - If s starts with a take one character then take at least one
                return 0;
            }
            if (((element.n_upper_case + element.n_lower_case) == 1) &&
                contains_any_of(element.m_data, '+', '-', '/', '*')) {
                return 0;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> read_math_inline(
    std::size_t pos,
    std::size_t initial_pos, /*where did the math inline block start*/
    const std::vector<element_v>& elements,
    std::vector<BlockElement>& history) {
    // FIXME - This is really not optimal...
    const auto& e = std::get<Element>(elements[pos]); 
    if (e.m_open_b_square || e.m_open_b_curly) {
        history.emplace_back(e.m_data);
        return pos;
    }
    else if (start_math_inline(e, history)) {
        history.emplace_back(e.m_data);
        return pos;
    }
    // TODO - this is awful
    else if (e.n_numerals == e.m_data.length()) {
        history.emplace_back(e.m_data);
        return pos;
    }
    else if (e.m_open_b_round != std::get<Element>(elements[initial_pos]).m_open_b_round) {
        // We must exit math mode with the same number of round brackets with which
        // we entered it
        history.emplace_back(e.m_data);
        return pos;
    }
    else if (
        !(e.n_lower_case == e.m_data.size()) &&
        pos + 1 < elements.size() && 
        std::holds_alternative<Element>(elements[pos + 1]) &&
        start_math_inline(std::get<Element>(elements[pos + 1]), history)) {
        history.emplace_back(std::get<Element>(elements[pos]).m_data);
        history.emplace_back(std::get<Element>(elements[pos + 1]).m_data);
        return pos + 1;
    }
    return std::nullopt;
}


size_t handle_line_break(
    const LineBreak& line_break,
    size_t pos,
    const std::vector<element_v>& elements,
    std::deque<Block>& history) {
    if (history.back().m_type == EnvironmentType::Array) {
        // An array must be completed on the same line
        throw Exception{
            "[",
            line_break.m_line_number,
            ":0] SyntaxError: Array started on previous line must be completed on the same line"
        };
    }
    if (line_break.is_empty) {
        switch (history.back().m_type)
        {
        case EnvironmentType::MathInline:
            // RULE: an empty line ends an inline environment
            push_last_block(history);
        case EnvironmentType::PlainText:
        case EnvironmentType::MathBlock:
        case EnvironmentType::ListItem:
            history.back().m_data.emplace_back("\n");
            break;
        default:
            assert(false);
        }
    }
    else
    {
        if (line_break.m_indent > history.back().m_indent) {
            if (auto item_label = try_get_item_label(pos + 1, elements)) {
                auto [label, end_of_label] = *item_label;
                history.emplace_back(
                    EnvironmentType::ListItem,
                    line_break.m_indent,  // TODO check if it is +4
                    pos + 1,
                    label,
                    history.back().m_type != EnvironmentType::ListItem ? "first" : ""
                );
                return end_of_label;
            }
            else {
                throw Exception{
                    "[",
                    line_break.m_line_number,
                    ":0] IndentationError: Expected indent ",
                    history.back().m_indent,
                    " found ",
                    line_break.m_indent
                };
            }
        }

        while (history.back().m_indent > line_break.m_indent) {
            push_last_block(history);
        }
        if (history.back().m_indent != line_break.m_indent) {
            throw Exception{
                "[",
                line_break.m_line_number,
                ":0] IndentationError: Indent level doesn't match any. Expecting ",
                history.back().m_indent,
                " started on line ",
                get_line_number(elements[history.back().m_initial_pos]),
                " found ",
                line_break.m_indent
            };

        }
    }
    return pos + 1;
}


size_t handle_environment_decleration(
    const EnvironmentDecleration& environment_decleration,
    size_t pos,
    const std::vector<element_v>& elements,
    std::deque<Block>& history) {
    switch (history.back().m_type)
    {
    case EnvironmentType::MathBlock:
    case EnvironmentType::Array:
        {
            throw Exception{
                "[",
                environment_decleration.m_line_number,
                ":*] SyntaxError: Cannot declare an environment whilst in environment ",
                history.back().m_type
            };
        }
    case EnvironmentType::MathInline:
        push_last_block(history);
    case EnvironmentType::PlainText:
    case EnvironmentType::ListItem:
        {
            switch(environment_decleration.m_type)
            {
            case EnvironmentType::Section:
                {
                    history.back().m_data.emplace_back(
                        "\\" +
                        environment_decleration.m_name + 
                        "{" +
                        environment_decleration.m_label +
                        "}"
                    );
                    break;
                }
            case EnvironmentType::PlainText:
            case EnvironmentType::MathBlock:
                {
                    history.emplace_back(
                        environment_decleration.m_type,
                        history.back().m_indent + 4,
                        pos,
                        environment_decleration.m_name,
                        environment_decleration.m_label
                    );
                    break;
                }
            default:
                {
                    throw Exception{
                        "[",
                        environment_decleration.m_line_number,
                        ":*] SyntaxError: Cannot declar environment of type ",
                        environment_decleration.m_type
                    };
                } 
            }
            break;
        }
    default:
        assert(false);
    }
    return pos + 1;
}

size_t handle_element(
    const Element& element,
    size_t pos,
    const std::vector<element_v>& elements,
    std::deque<Block>& history) {
    switch (history.back().m_type)
    {
    case EnvironmentType::ListItem:
        {
            if (auto item_label = try_get_item_label(pos, elements)) {
                auto [label, end_of_label] = *item_label;
                history.emplace_back(
                    EnvironmentType::ListItem,
                    history.back().m_indent + 4,
                    pos,
                    label,
                    ""
                );
                return end_of_label;
            }
        }
    case EnvironmentType::PlainText:
        {
            if (auto math_start_opt = start_math_inline(element, history.back().m_data)) {
                // FIXME - check here for bracket numbers and raise exception if have curly or
                // square. In the case that we have a round we need to check that we exit with the
                // same number

                // If math_start > 0 we need to take that many elements from the history
                // We first create the block and then add in the data
                size_t math_start = *math_start_opt;
                Block block = {
                    EnvironmentType::MathInline,
                    history.back().m_indent,
                    pos - math_start, // TODO - not sure actually correct
                    "",
                    ""
                };

                for (;math_start > 0; --math_start) {
                    block.m_data.emplace_back(history.back().m_data.back());
                    history.back().m_data.pop_back();
                }

                block.m_data.emplace_back(element.m_data);

                history.emplace_back(block);
            }
            else {
                // TODO - could look at neateing up words somehow
                history.back().m_data.emplace_back(element.m_data);
            }
            break;
        }
    case EnvironmentType::MathBlock:
        {
            // 1. Check if the array starts
            if (element.m_data == "\\arr{") {
                // TODO - figure out how to change bracket type...
                // could be something like \arr(, \arr[, etc.
                history.emplace_back(
                    EnvironmentType::Array,
                    history.back().m_indent,
                    pos,
                    "",
                    ""
                );
            }
            else {
                history.back().m_data.emplace_back(element.m_data);
            }

            break;
        }
    case EnvironmentType::Array:
        {
            if (element.m_data == "}") {
                // Array is complete
                push_last_block(history);
            }
            else {
                history.back().m_data.emplace_back(element.m_data);
            }

            break;
        }
    case EnvironmentType::MathInline:
        {
            if (auto pos_opt = read_math_inline(
                    pos,
                    history.back().m_initial_pos,
                    elements,
                    history.back().m_data))
            {
                // In this case we stay in math inline mode
                pos = *pos_opt;
            }
            else {
                push_last_block(history);
                history.back().m_data.emplace_back(element.m_data);
            }
            break;
        }
    default:
        assert(false);
    }
    return pos + 1;
}

std::string convert_to_tex(const std::vector<element_v>& elements) {
    using namespace std;

    size_t pos = 0;

    deque<Block> history;
    history.emplace_back(EnvironmentType::PlainText, 0, 0, "", "");

    while (pos < elements.size()) {
        pos = visit(
            [&](auto&& a) -> size_t {
                using T = decay_t<decltype(a)>;
                if constexpr (is_same_v<T, LineBreak>) {
                    return handle_line_break(a, pos, elements, history);
                }
                else if constexpr (is_same_v<T, EnvironmentDecleration>) {
                    return handle_environment_decleration(a, pos, elements, history);
                }
                else {
                    static_assert(is_same_v<T, Element>);
                    return handle_element(a, pos, elements, history);
                }
            },
            elements[pos]
        );
    }

    while (history.size() > 1) {
        push_last_block(history);
    }

    assert(history.size() == 1);

    auto final_block = amalgamate_block(history.back());
    return final_block.front(); // Everything else is just new lines
}

std::vector<element_v> read_file(std::string f_name) {
    using namespace std;

    string line;
    ifstream f_handle{f_name};

    size_t line_number = 0;
    // TODO - could get nicer error messages if kept track of where the last open brace
    // was , i.e. a stack of braces rather than a count;
    size_t open_b_round = 0;
    size_t open_b_square = 0;
    size_t open_b_curly = 0;

    std::vector<element_v> elements;

    while (++line_number, getline(f_handle, line)) {

        // 1. If it matches any lines we use for meta data we do not include in the elements
        // 2. Each new line is an element. If all white space we treat as a blank line
        // 3. An environment decleration is a single element.
        //    (i) It cannot happen if we have any open braces
        // 4. General element construction

        // 1. ...
        smatch match;

        if (regex_match(line, match, patterns::tag)) {
            continue;
        }

        if (regex_match(line, match, patterns::cmd)) {
            continue;
        }

        // TODO - comments

        // 2. ...
        size_t indent = line.find_first_not_of(' ');
        if (indent == string::npos) {
            // Line is empty
            elements.emplace_back(LineBreak{line_number, true, 0});
            continue;
        }

        elements.emplace_back(LineBreak{line_number, false, indent});

        // 3. ...
        if (regex_match(line, match, patterns::env_decl) ||
            regex_match(line, match, patterns::section_def)) {
            string short_name = match[1];
            if (auto it = s_environments.find(short_name); it != s_environments.end()) {
                if (open_b_round || open_b_square || open_b_curly) {
                    throw Exception{
                        "[",
                        line_number,
                        ":0] SyntaxError: Cannot start new environment='",
                        short_name,
                        "' as brackets not closed. Open '('=",
                        open_b_round,
                        ", '{'=",
                        open_b_curly,
                        ", '['=",
                        open_b_square,
                        "."
                    };
                }

                auto [env_type, name] = it->second;
                string label = match[2];
                elements.emplace_back(EnvironmentDecleration{line_number, env_type, name, label});
                continue;
            }
        }

        // 4. Now we loop over over the characters in the line and do the big switch statement
        auto element_f = [&](size_t pos, string data) {
            size_t n_lower_case = 0, n_upper_case = 0, n_numerals = 0;
            for (char c : data) {
                if (c >= 'a' && c <= 'z') ++n_lower_case;
                if (c >= 'A' && c <= 'Z') ++n_upper_case;
                if (c >= '0' && c <= '9') ++n_numerals;
            }

            return Element{
                line_number,
                pos,
                open_b_round,
                open_b_square,
                open_b_curly,
                n_lower_case,
                n_upper_case,
                n_numerals,
                data
            };
        };

        auto write_element_f = [&](size_t start, size_t end) {
            if (start < end) {
                elements.emplace_back(element_f(start, line.substr(start, end - start)));
            }
        };

        size_t w_begin = indent;

        for (size_t pos = indent; pos < line.size(); ++pos) {
            char c = line[pos];

            switch (c)
            {
            case ' ':
                {
                    write_element_f(w_begin, pos);
                    w_begin = pos + 1;
                    break;
                }
            case '{':
                {
                    write_element_f(w_begin, pos + 1);
                    ++open_b_curly;
                    w_begin = pos + 1;
                    break;
                }
            case '}':
                {
                    write_element_f(w_begin, pos);
                    elements.emplace_back(element_f(pos, "}"));
                    --open_b_curly;
                    w_begin = pos + 1;
                    break;
                }
            case '[':
                {
                    write_element_f(w_begin, pos);
                    elements.emplace_back(element_f(pos, "["));
                    ++open_b_square;
                    w_begin = pos + 1;
                    break;

                }
            case ']':
                {
                    write_element_f(w_begin, pos);
                    elements.emplace_back(element_f(pos, "]"));
                    --open_b_square;
                    w_begin = pos + 1;
                    break;

                }
            case '(':
                {
                    write_element_f(w_begin, pos);
                    elements.emplace_back(element_f(pos, "("));
                    ++open_b_round;
                    w_begin = pos + 1;
                    break;

                }
            case ')':
                {
                    write_element_f(w_begin, pos);
                    elements.emplace_back(element_f(pos, ")"));
                    --open_b_round;
                    w_begin = pos + 1;
                    break;

                }
            case ';':
                {
                    write_element_f(w_begin, pos + 1);
                    w_begin = pos + 1;
                    break;
                }
            case '+':
            case '*':
            case '/':
                {
                    write_element_f(w_begin, pos);
                    elements.emplace_back(element_f(pos, string{c}));
                    w_begin = pos + 1;
                    break;
                }
            default:
                break;
            }
        }

        if (w_begin < line.size()) {
            elements.emplace_back(
                element_f(w_begin, line.substr(w_begin))
            );
        }
    }

    return elements;
}


std::string get_ntx_info() {

    std::time_t now = std::time(nullptr);

    std::stringstream ss;
    ss << "\n\n% "
       << "ntx["
       << NTX_VERSION_MAJOR
       << ":"
       << NTX_VERSION_MINOR
       << "] - compiled @ "
       << std::asctime(std::gmtime(&now));

    return ss.str();
}

} // ntx




int main(int argc, char** argv) {
    namespace po = boost::program_options;
    po::options_description desc{"Allowed options."};
    desc.add_options()
        ("help",                              "Produce help message")
        ("file,f",  po::value<std::string>(), "Which ntx to compile to tex")
        ("out,o",   po::value<std::string>(), "If set write to the passed file")
        ("debug,d",                           "Print the elements to screen")
    ;

    po::variables_map vm{};
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    if (vm.count("file")) {

        std::optional<std::string> tex_data;
        try {
            auto elements = ntx::read_file(vm["file"].as<std::string>());

            tex_data = convert_to_tex(elements);

            if (vm.count("debug")) {
                using namespace std;
                using namespace ntx;

                for (const auto& element : elements) {
                    visit(
                        [](auto&& a) {
                            using T = std::decay_t<decltype(a)>;

                            if constexpr (std::is_same_v<T, LineBreak>) {
                                cout << "["
                                     << a.m_line_number
                                     << "]  LineBreak{is_empty="
                                     << a.is_empty
                                     << ", indent="
                                     << a.m_indent
                                     << "}\n";
                            }
                            if constexpr (std::is_same_v<T, EnvironmentDecleration>) {
                                cout << "["
                                     << a.m_line_number
                                     << "]  EnvironmentDecleration{name="
                                     << a.m_name
                                     << ", label="
                                     << a.m_label
                                     << "}\n";

                            }
                            if constexpr (std::is_same_v<T, Element>) {
                                cout << "["
                                     << a.m_line_number
                                     << ":"
                                     << a.m_line_pos
                                     << "]  Element{open_b_round="
                                     << a.m_open_b_round
                                     << ", open_b_square="
                                     << a.m_open_b_square
                                     << ", open_b_curly="
                                     << a.m_open_b_curly
                                     << ", n_lower_case="
                                     << a.n_lower_case
                                     << ", n_upper_case="
                                     << a.n_upper_case
                                     << ", n_numerals="
                                     << a.n_numerals
                                     << ", data=\""
                                     << a.m_data
                                     << "\"}\n";

                            }
                        },
                        element
                    );
                }
            }
        }

        catch (const ntx::Exception& e) {
            std::cout << e.m_message << std::endl;
            std::cout << "[FATAL] Cannot continue compiling" << std::endl;
            return 2;
        }

        if (!tex_data) {
            std::cout << "Internal error - no tex data produced" << std::endl;
            std::cout << "[FATAL] Cannot continue compiling" << std::endl;
            return 2;
        }

        if (vm.count("out")) {
            std::ofstream out{vm["out"].as<std::string>()};
            out << *tex_data << ntx::get_ntx_info();
        }
        else {
            std::cout << *tex_data << ntx::get_ntx_info();
        }

        // FIXME - need to have the option to add preamble to one tex file,
        //       - to be able to compile multiple files,
        //       - ...
    }
}
