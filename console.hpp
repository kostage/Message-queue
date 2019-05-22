
#pragma once

#include <iostream>

namespace ZodiacTest {

    #ifdef DEBUG
    template<typename ... Strings>
    void logConsole( Strings&& ... strs )
    {
        (std::clog << ... << std::forward<Strings>(strs));
    }
    #else
    template<typename ... Strings>
    void logConsole( Strings&& ... strs ) 
    {
    }
    
    #endif

} // namespace ZodiacTest
