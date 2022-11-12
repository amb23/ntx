
#include <iostream>  // TODO is this correct

#include "blox.hpp"

namespace ntx::blox {

const std::unordered_map<std::string, std::function<Block(std::string)>>
s_block_factories = {
    {"proof",        [](std::string l) -> Block { return SimpleEnvironment{"proof", l}; }},
    {"thm",          [](std::string l) -> Block { return SimpleEnvironment{"theorem", l}; }},
    {"lemma",        [](std::string l) -> Block { return SimpleEnvironment{"lemma", l}; }},
    {"prop",         [](std::string l) -> Block { return SimpleEnvironment{"proposition", l}; }},
    {"construction", [](std::string l) -> Block { return SimpleEnvironment{"construction", l}; }},
    {"eq",           [](std::string l) -> Block { return BlockEquation{l}; }}
    // FIXME {"list",         {"enumerate",    EnvironmentType::List}}
};

template <typename T>
void write_data_with_sep(std::ostream& os, const std::vector<T>& data, char sep) {
    std::size_t n_to_write = data.size();
    os << data[0];
    for (std::size_t i = 1; i < n_to_write; ++i) {
        os << sep << data[i];
    }
}

void write_tex(
    std::ostream& os,
    const std::string& name,
    const std::string& label,
    const std::vector<std::string>& data) {
    os << "\\begin{" << name << "}\n";
    if (label.length()) {
        os << "\\label{" << label << "}\n";
    }
    write_data_with_sep(os, data, ' ');
    os << "\n\\end{" << name << "}\n";

}

void write_tex(
    std::ostream& os,
    const SimpleEnvironment& block,
    const std::vector<std::string>& data) {
    write_tex(os, block.m_name, block.m_label, data);
}

void write_tex(std::ostream& os, const BlockEquation& block, const std::vector<std::string>& data) {
    write_tex(os, block.m_label.length() ? "equation" : "equation*", block.m_label, data);
}

void write_tex(std::ostream& os, const Array& block, const std::vector<std::string>& data) {

    std::size_t current_row_size = 0;
    std::size_t max_row_size = 0;
    for (const auto& s : data) {
        if (s == ";") {
            max_row_size = std::max(max_row_size, current_row_size);
            current_row_size = 0;
        }
        else {
            ++current_row_size;
        }
    }

    os  << "\\left( \\begin{array}{"
        << std::string(max_row_size, 'c')
        << "} ";

    std::size_t row_pos = 0;
    for (const auto& element : data) {
        if (element == ";") {
            os << "// ";
            row_pos = 0;
        }
        else {
            os << element << (++row_pos == max_row_size ? " " : " & ");
        }
    }

    os << "\\end{array} \\right)";
}


void write_tex(
    std::ostream& os, const InlineEquation& block, const std::vector<std::string>& data) {
    os << "$";
    write_data_with_sep(os, data, ' ');
    os << "$";
}


void write_tex(std::ostream& os, const Block& block, const std::vector<std::string>& data) {
    std::visit([&os, &data](auto&& a) { write_tex(os, a, data); }, block);
}

} // ntx::blox
