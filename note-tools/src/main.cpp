#include <deque>
#include <exception>
#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <sstream>

#include <boost/program_options.hpp>

#include "blox.hpp"
#include "parsers.hpp"

namespace ntx {

namespace patterns {

static const std::regex tag{"^% TAG (.+)"};
static const std::regex cmd{"^% CMD (.+) (.+)"};
static const std::regex env_decl{"^[ ]*\\\\([A-z]+)[ ]*([^ ]*)"};

} // patterns


void compile_to_tex(std::string f_name, std::ostream& out) {
    using namespace std;

    using namespace blox;
    using namespace parsers;

    ParsedData history;
    history.emplace_back(SimpleEnvironment{"__global__", ""}, 0);


    string line;
    ifstream f_handle{f_name};

    size_t line_no = 0;

    while (++line_no, getline(f_handle, line)) {
        smatch match;

        if (regex_match(line, match, patterns::tag)) {
            // TODO - store
            continue;
        }

        if (regex_match(line, match, patterns::cmd)) {
            // TODO - store
            continue;
        }

        size_t indent = line.find_first_not_of(' ');
        parse_new_line(indent, indent == string::npos ? 0 : line.length(), history);

        if (regex_match(line, match, patterns::env_decl)) {
            if (auto it = s_block_factories.find(match[1]);
                it != s_block_factories.end()) {
                std::string label = match[2];

                parse_new_env(it->second(label), history);
                continue;
            }

            // log warning maybe
        }

        // Do you want to add the line_no/line_pos?
        while (indent < string::npos) {
            size_t end_of_word = line.find_first_of(' ', indent);
            parse_element(line.substr(indent, end_of_word - indent), history);
            indent = line.find_first_not_of(' ', end_of_word);
        }
    }

    // unwind to last element
    while (history.size() > 1) {
        pop_back(history);
    }

    for (const auto& s : history.back().m_elements) {
        out << s << ' ';
    }
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
        if (vm.count("out")) {
            std::ofstream out{vm["out"].as<std::string>()};
            ntx::compile_to_tex(vm["file"].as<std::string>(), out);
        }
        else {
            ntx::compile_to_tex(vm["file"].as<std::string>(), std::cout);
        }
    }
}
