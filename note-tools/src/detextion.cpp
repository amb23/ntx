
#include <regex>

#include "detextion.hpp"


namespace ntx::detextion {

namespace {

const std::regex ref{"\\ref.*"};
const std::regex number_p{".*[0-9]+.*"};

bool contains(const std::string& s, char c) {
    return s.find(c) != std::string::npos;
}


int n_alnum(const std::string& s) {
    int n = 0;
    for (char c : s) { n += std::isalnum(static_cast<unsigned char>(c)) ? 1 : 0; }
    return n;
}

bool is_char_in_math_mode(char c, bool currently_in_math_mode) {
    // We take all single alpha-numeric characters except 'a' and 'I'
    if (std::isalnum(static_cast<unsigned char>(c)) && c != 'a' && c != 'I') {
        return true;
    }

    if (c == '=' || c == '-' || c == '+' || c == '/' || c == '*') {
        return true;
    }

    return false;
}

bool is_string_in_math_mode(const std::string& s, bool currently_in_math_mode) {
    std::smatch match;

    if (std::regex_match(s, match, ref)) {
        return false;
    }

    if (s.starts_with('\\') || contains(s, '^') || contains(s, '_')) {
        return true;
    }

    if (std::regex_match(s, match, number_p)) {
        return currently_in_math_mode;
    }

    // If it contains only a single alpha numeric character then it is likely to 
    // be in math mode
    if (n_alnum(s) <= 1) {
        return currently_in_math_mode;
    }

    return false;
}


} //


bool is_math_mode(const std::string& s, bool currently_in_math_mode) {
    switch (s.length())
    {
    case 0:
        return currently_in_math_mode;
    case 1:
        return is_char_in_math_mode(s[0], currently_in_math_mode);
    default:
        return is_string_in_math_mode(s, currently_in_math_mode);
    }
}


} // ntx::detextion
