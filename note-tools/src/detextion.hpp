#pragma once

#include <string>

namespace ntx::detextion {

// TODO - parse the full text to try and idnetify structure, need some ML probs

    // TODO - potentially pass in the history at the current zone and return an
    // int which includes how far back to add into the inline
bool is_math_mode(const std::string&, bool currently_in_math_mode);

} // ntx::detextion

