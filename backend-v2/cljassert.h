#ifndef CLJASSERT_H
#define CLJASSERT_H

#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
#include <execinfo.h>    // for backtrace() and backtrace_symbols()
#include <cstdlib>       // for free()

inline std::string getStackTrace(unsigned int maxFrames = 200) {
    void* addrlist[maxFrames+1];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, (int)(sizeof(addrlist)/sizeof(void*)));

    if (addrlen == 0) {
        return "  <empty, no stack trace>\n";
    }

    // Convert addresses into an array of strings (mangled symbols)
    char** symbollist = backtrace_symbols(addrlist, addrlen);

    if (!symbollist) {
        return "  <no symbols available>\n";
    }

    std::ostringstream oss;
    for (int i = 0; i < addrlen; i++) {
        oss << "  " << symbollist[i] << "\n";
    }

    free(symbollist);
    return oss.str();
}

#define CLJ_ASSERT(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::ostringstream oss;                                            \
            oss << "Assertion failed: (" #cond ") " << msg                     \
                << "\nFile: " << __FILE__ << "\nLine: " << __LINE__ << "\n"    \
                << "Stack trace:\n" << getStackTrace();                        \
            throw std::runtime_error(oss.str());                               \
        }                                                                      \
    } while (false)


#endif
