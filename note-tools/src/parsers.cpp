
#include <assert.h>
#include <sstream>
#include <regex>

#include "blox.hpp"
#include "parsers.hpp"

namespace ntx::parsers {

namespace {

static const std::regex start_of_array("\\arr\\{(.*)");
static const std::regex end_of_array("(.*)\\}");

}

bool contains(const std::string& s, char c) { return s.find(c) != std::string::npos; }

// This is where we want to add some logic in the future
bool is_likely_in_equation(const std::string& s) {
    return contains(s, '^') || 
           contains(s, '_') || 
           contains(s, '[') || 
           contains(s, ']') || 
           contains(s, '}') || 
           contains(s, '{') || 
           s.starts_with('\\') || 
           contains(s, '=') ||
           contains(s, '+') ||
           s == "-" ||
           contains(s, '/');
}

char get_array_bracket_type(const std::string&) { return '('; };


void add_array_data(std::string s, std::vector<std::string>& data) {
    std::size_t pos = 0;
    while (pos != std::string::npos) {
        std::size_t it = s.find_first_of(';', pos);
        if (it != pos && pos < s.length()) {
            data.emplace_back(s.substr(pos, it - pos));
        }
        if (it != std::string::npos) {
            data.emplace_back(";");
            pos = it + 1;
        }
        else
        {
            break;
        }
    }
}

void parse_element(const blox::Array&, std::string element, ParsedData& history) {
    std::smatch match;
    if (std::regex_match(element, match, end_of_array)) {
        std::string extra_data = match[1];
        add_array_data(extra_data, history.back().m_elements);

        pop_back(history);
    }
    else {
        add_array_data(element, history.back().m_elements);
    }

    // FIXME - do we want arrays in arrays? If not check and throw
}


void parse_element(const blox::SimpleEnvironment&, std::string element, ParsedData& history) {
    if (is_likely_in_equation(element)) {
        // Could peek back to see if we should include, maybe is_likely_in_equation could return
        // an enum
        history.emplace_back(blox::Block{blox::InlineEquation{}}, history.back().m_indent);
    }
    history.back().m_elements.emplace_back(element);
}

// TODO - if you want to allow \arr  { as well as \arr{ you may need to have
// a peek next element
void parse_element(blox::BlockEquation, std::string element, ParsedData& history) {
    using namespace blox;
    if (element.starts_with("\\arr{")) {
        char bracket_type = get_array_bracket_type(element);
        history.emplace_back(
            blox::Block{blox::Array{bracket_type}}, history.back().m_indent
        );

        if (element.length() > 5) {
            add_array_data(element.substr(5), history.back().m_elements);
        }
    }
    else {
        history.back().m_elements.emplace_back(element);
    }
}

// Would be nice to use this as a template rather than as 
void parse_element(blox::InlineEquation, std::string element, ParsedData& history) {
    if (!is_likely_in_equation(element)) {
        pop_back(history);
    }

    history.back().m_elements.push_back(element);
}

void parse_element(std::string element, ParsedData& history) {
    // FIXME - could od a constexpr loop instead of passing in all the junk
    std::visit(
        [&history, element](auto&& a) { parse_element(a, element, history); },
        history.back().m_data);
}


void parse_new_line(std::size_t indent, std::size_t length, ParsedData& history) {
    std::visit(
        [&history, indent, length](auto&& a) {
            using T = std::decay_t<decltype(a)>;

            if constexpr (std::is_same_v<T, blox::Array>) {
                // FIXME throw exception
            }

            if (!length) {
                if constexpr (std::is_same_v<T, blox::InlineEquation>) {
                    pop_back(history);
                }
                if constexpr (std::is_same_v<T, blox::SimpleEnvironment> ||
                              std::is_same_v<T, blox::BlockEquation>) {
                    // We only store at most two new lines in a row. 
                    // FIXME - if we end with a new line we should actually push it up to the
                    // parent class?
                    auto& elements = history.back().m_elements;
                    std::size_t n = elements.size();
                    if (n < 1 || elements[n - 1] != "\n" || elements[n - 2] != "\n") {
                        elements.emplace_back("\n");
                    }
                }
            }
            else {
                if (indent > history.back().m_indent) {
                    // FIXME throw unexpected indent
                }
                while (indent < history.back().m_indent) {
                    pop_back(history);
                }
                // FIXME - actually check the below and then throw.
                assert (indent == history.back().m_indent);
            }
        },
        history.back().m_data
    );
}


void parse_new_env(blox::Block block, ParsedData& history) {
    using namespace blox;
    std::visit(
        [&history](auto&& a) {
            using T  = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, Array> ||
                          std::is_same_v<T, BlockEquation>) {
                // FIXME - throw exception
                // In the case of a block equation we have that a new env
                // must be at a lower level
            }
            if constexpr (std::is_same_v<T, InlineEquation>) {
                pop_back(history);
            }
        },
        history.back().m_data);

        history.emplace_back(block, history.back().m_indent + 4);
}


void pop_back(ParsedData& history) {
    std::stringstream ss;
    write_tex(ss, history.back().m_data, history.back().m_elements);
    history.pop_back();
    history.back().m_elements.emplace_back(ss.str());
}

} // ntx::parsers
