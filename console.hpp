
#pragma once

#include <iostream>

namespace zodiactest {

    #ifdef DEBUG
    template<typename String>
    void logConsole(String&& str)
    {
        std::clog << std::forward<String>(str);
    }
    #else
    template<typename String>
    void logConsole(String&&)
    {
    }
    
    #endif

} // namespace zodiactest
