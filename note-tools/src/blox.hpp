#pragma once

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace ntx::blox {

struct EmptyData {};

struct ListData
{
    using item_meta = std::tuple<std::string, std::size_t>; // format, pos
    std::vector<item_meta> m_item_meta;
};

// FIXME array parser should write out something like 4;4;; as [4 ; 4 ; ;]
struct Array
{
    // FIXME think about syntax for other brackets
    // FIXME - use this
    char m_bracket_type;
};


struct SimpleEnvironment
{
    std::string m_name;
    std::string m_label;
};

struct Section
{
    std;:string m_name;
};

struct BlockEquation
{
    std::string m_label;
};

struct InlineEquation {};

// FIXME - List

using Block = std::variant<
    SimpleEnvironment,
    BlockEquation,
    Array,
    InlineEquation
>;


void write_tex(std::ostream&, const Block&, const std::vector<std::string>&);


// TODO - make non-const and allow users to define their own envs
extern const std::unordered_map<std::string, std::function<Block(std::string)>> s_block_factories;

} // ntx::blox
