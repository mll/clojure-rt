#ifndef RT_SOURCE_LOCATION_H
#define RT_SOURCE_LOCATION_H

#include <string>

namespace rt {

struct SourceLocation {
  std::string file;
  int line;
  int column;

  bool operator<(const SourceLocation &other) const {
    if (file != other.file) return file < other.file;
    if (line != other.line) return line < other.line;
    return column < other.column;
  }
};

} // namespace rt

#endif
