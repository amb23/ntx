#pragma once

#include <deque>
#include <string>
#include <variant>

#include "blox.hpp"

namespace ntx::parsers {

struct ParsingBlock
{
    blox::Block m_data;
    std::size_t m_indent;
    std::vector<std::string> m_elements = {};
};

using ParsedData = std::deque<ParsingBlock>;


void parse_element(std::string element, ParsedData& history);

void parse_new_line(std::size_t indent, std::size_t length, ParsedData& history); 

void parse_new_env(blox::Block, ParsedData& history);

void pop_back(ParsedData&);

} // ntx::parsers

