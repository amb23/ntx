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

//#include "blox.hpp"
//#include "parsers.hpp"
//#include "types.hpp"

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
    List,
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
    case EnvironmentType::List:
        os << "List";
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

    /* FIXME - probably want to store earlier
    std::size_t n_lower_case;
    std::size_t n_upper_case;
    std::size_t n_numerals;
    */

    std::string m_data;
};


using element_v = std::variant<LineBreak, EnvironmentDecleration, Element>;


std::size_t get_line_number(const element_v& element) {
    return std::visit([](auto&& a) { return a.m_line_number; }, element);
}


using env_data_t = std::tuple<EnvironmentType, std::string>;

static const std::unordered_map<std::string, env_data_t> s_environments = {
    {"proof",         {EnvironmentType::PlainText, "proof"}},
    {"thm",           {EnvironmentType::PlainText, "thm"}},
    {"lemma",         {EnvironmentType::PlainText, "lemma"}},
    {"prop",          {EnvironmentType::PlainText, "proposition"}},
    {"construction",  {EnvironmentType::PlainText, "construction"}},
    {"eq",            {EnvironmentType::MathBlock, "equation"}},
    {"section",       {EnvironmentType::Section, "section"}},
    {"subsection",    {EnvironmentType::Section,"subsection"}},
    {"subsubsection", {EnvironmentType::Section,"subsubsection"}},
    {"list",          {EnvironmentType::List,"enumerate"}}
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
    // the data on completion (or maybe srting_view....)
    std::vector<BlockElement> m_data = {};
};

std::string amalgamate_block(const Block& block) {
    std::string amalgamated;
    switch (block.m_type)
    {
    case EnvironmentType::PlainText:
        {
            if (block.m_name.length()) {
                amalgamated += "\n\\begin{" + block.m_name + "}\n";
                if (block.m_label.length()) {
                    amalgamated += "\\label{" + block.m_label + "}\n";
                }
            }
            for (const auto& element : block.m_data) {
                // FIXME - clean up this loop
                amalgamated += element.m_data + ' ';
            }
            if (block.m_name.length()) {
                // FIXME - check if last line was a new line, don't want two
                amalgamated += "\n\\end{" + block.m_name + "}\n";
            }

            break;
        }
    case EnvironmentType::MathBlock:
        {
            bool has_label = block.m_label.length();
            std::string name = has_label ? "equation" : "equation*";
            amalgamated += "\n\\begin{" + name + "}\n";
            if (has_label) {
                amalgamated += "\\label{" + block.m_label + "}\n";
            }
            for (const auto& element : block.m_data) {
                // FIXME - clean up this loop
                amalgamated += element.m_data + ' ';
            }
            // FIXME - check if last line was a new line, don't want two
            amalgamated += "\n\\end{" + name + "}\n";
            break;
        }
    case EnvironmentType::MathInline:
        {
            amalgamated += "$";
            for (const auto& element : block.m_data) {
                // FIXME - clean up this loop
                amalgamated += element.m_data + ' ';
            }
            // FIXME Inline check if last point is grammar or not
            amalgamated += "$";
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

            amalgamated += "\\left\\( \\begin{array}{" + std::string(max_row_length, 'c') + "} ";
            for (const auto& e : block.m_data) {
                if (e.m_data.ends_with(';')) {
                    if (auto n_e = e.m_data.length(); n_e > 1) {
                        amalgamated += e.m_data.substr(0, n_e - 1) + " ";
                    }
                    amalgamated += "// ";
                    current_row_length = 0;
                }
                else {
                    amalgamated += e.m_data + (++current_row_length < max_row_length ? " & " : " ");
                }
            }
            amalgamated += "\\end{array} \\right\\)";
            break;
        }
    default:
        assert(false); // FIXME - do the lists...
    /*
    Section,
    List,
    ListItem
    */
    }
    return amalgamated;
}

void push_last_block(std::deque<Block>& history) {
    // FIXME - check that all brackets are closed...
    auto& block = history.back();
    auto amalgamated = amalgamate_block(block);
    history.pop_back();
    history.back().m_data.emplace_back(amalgamated);
}

using item_label_t = std::tuple<std::string, std::size_t /* end of label */>;

std::optional<item_label_t> try_get_item_label(std::size_t pos, const std::vector<element_v>&) {
    return std::nullopt;
}

std::optional<std::size_t> start_math_inline(const Element&, const std::vector<BlockElement>&) {
    return std::nullopt;
}

std::optional<std::size_t> read_math_inline(std::size_t pos, const std::vector<element_v>&, const std::vector<BlockElement>&) {
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
        case EnvironmentType::List:
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
            throw Exception{
                "[",
                line_break.m_line_number,
                ":0] IndentationError: Expected indent ",
                history.back().m_indent,
                " found ",
                line_break.m_indent
            };
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
    case EnvironmentType::List:
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
            case EnvironmentType::List:
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
    case EnvironmentType::List:
        {
            if (auto item_label_opt = try_get_item_label(pos, elements)) {
                auto [item_label, label_end] = *item_label_opt;
                history.emplace_back(
                    EnvironmentType::ListItem,
                    history.back().m_indent + 4,
                    pos,
                    item_label,
                    ""
                );
                pos = label_end;
            }
            else {
                throw Exception{
                    "[",
                    element.m_line_number,
                    ":",
                    element.m_line_pos,
                    "] SyntaxError: Expecting a new item in list mode but couldn't parse any."
                };
            }
        }
    case EnvironmentType::ListItem:
    case EnvironmentType::PlainText:
        {
            if (auto math_start_opt = start_math_inline(element, history.back().m_data)) {
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
            // FIXME - not sure what the point of the inline math mode is // maybe new lines etc.
            // FIXME - we need to have the option of breaking mathinline here 
            if (auto pos_opt = read_math_inline(pos, elements, history.back().m_data))
            {
                // In this case we stay in math inline mode
                pos = *pos_opt;
            }
            else {
                push_last_block(history);
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
        // pos = handle_element(pos, elements, history);
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

    return amalgamate_block(history.back());
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
        if (regex_match(line, match, patterns::env_decl)) {
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
            return Element{
                line_number,
                pos,
                open_b_round,
                open_b_square,
                open_b_curly,
                data
            };
        };

        size_t w_begin = indent;

        for (size_t pos = indent; pos < line.size(); ++pos) {
            char c = line[pos];

            switch (c)
            {
            case ' ':
                {
                    if (w_begin < pos) {
                        elements.emplace_back(
                            element_f(w_begin, line.substr(w_begin, pos - w_begin))
                        );
                    }

                    w_begin = pos + 1;
                    break;
                }
            case '{':
                {
                    elements.emplace_back(
                        element_f(w_begin, line.substr(w_begin, pos + 1 - w_begin))
                    );

                    ++open_b_curly;
                    w_begin = pos + 1;
                    break;
                }
            case '}':
                {
                    if (w_begin < pos) {
                        elements.emplace_back(
                            element_f(w_begin, line.substr(w_begin, pos - w_begin))
                        );
                    }

                    elements.emplace_back(element_f(pos, "}"));
                    --open_b_curly;
                    w_begin = pos + 1;
                    break;
                }
            case '[':
                {
                    if (w_begin < pos) {
                        elements.emplace_back(
                            element_f(w_begin, line.substr(w_begin, pos - w_begin))
                        );
                    }

                    elements.emplace_back(element_f(pos, "["));
                    ++open_b_square;
                    w_begin = pos + 1;
                    break;

                }
            case ']':
                {
                    if (w_begin < pos) {
                        elements.emplace_back(
                            element_f(w_begin, line.substr(w_begin, pos - w_begin))
                        );
                    }

                    elements.emplace_back(element_f(pos, "]"));
                    --open_b_square;
                    w_begin = pos + 1;
                    break;

                }
            case '(':
                {
                    if (w_begin < pos) {
                        elements.emplace_back(
                            element_f(w_begin, line.substr(w_begin, pos - w_begin))
                        );
                    }

                    elements.emplace_back(element_f(pos, "("));
                    ++open_b_round;
                    w_begin = pos + 1;
                    break;

                }
            case ')':
                {
                    // FIXME - should make this a function or something
                    if (w_begin < pos) {
                        elements.emplace_back(
                            element_f(w_begin, line.substr(w_begin, pos - w_begin))
                        );
                    }

                    elements.emplace_back(element_f(pos, ")"));
                    --open_b_round;
                    w_begin = pos + 1;
                    break;

                }
            case ';':
                {
                    elements.emplace_back(
                        element_f(w_begin, line.substr(w_begin, pos + 1 - w_begin))
                    );

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

} // ntx




int main(int argc, char** argv) {
    namespace po = boost::program_options;
    po::options_description desc{"Allowed options."};
    desc.add_options()
        ("help",                              "Produce help message")
        ("file,f", po::value<std::string>(), "Which ntx to compile to tex")
        ("out,o",  po::value<std::string>(), "If set write to the passed file")
    ;

    po::variables_map vm{};
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    if (vm.count("file")) {
        try {
            auto elements = ntx::read_file(vm["file"].as<std::string>());

            auto tex = convert_to_tex(elements);
            std::cout << tex << std::endl;


            using namespace std;
            using namespace ntx;

            /*
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
                                 << ", data=\""
                                 << a.m_data
                                 << "\"}\n";

                        }
                    },
                    element
                );
            }
            */
        }

        catch (const ntx::Exception& e) {
            std::cout << e.m_message << std::endl;
            std::cout << "[FATAL] Cannot continue compiling" << std::endl;
            return 2;
        }

        /*
        if (vm.count("out")) {
            std::ofstream out{vm["out"].as<std::string>()};
            ntx::compile_to_tex(vm["file"].as<std::string>(), out);
        }
        else {
            ntx::compile_to_tex(vm["file"].as<std::string>(), std::cout);
        }*/
    }
}
