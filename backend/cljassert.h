#ifndef CLJASSERT_H
#define CLJASSERT_H

#include <cstdlib>    // for free()
#include <execinfo.h> // for backtrace() and backtrace_symbols()
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

inline std::string getStackTrace(unsigned int maxFrames = 200) {
  // Avoid VLAs (Clang extension in C++) by using a fixed-size buffer.
  // 256 frames is more than enough for diagnostic traces.
  static const int kMaxFrames = 256;
  void *addrlist[kMaxFrames];
  unsigned int actualMax = (maxFrames < kMaxFrames) ? maxFrames : kMaxFrames;

  // retrieve current stack addresses
  int addrlen = backtrace(addrlist, actualMax);

  if (addrlen == 0) {
    return "  <empty, no stack trace>\n";
  }

  // Convert addresses into an array of strings (mangled symbols)
  char **symbollist = backtrace_symbols(addrlist, addrlen);

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
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::ostringstream oss;                                                  \
      oss << "Assertion failed: (" #cond ") " << msg << "\nFile: " << __FILE__ \
          << "\nLine: " << __LINE__ << "\n"                                    \
          << "Stack trace:\n"                                                  \
          << getStackTrace();                                                  \
      throw std::runtime_error(oss.str());                                     \
    }                                                                          \
  } while (false)

#endif
