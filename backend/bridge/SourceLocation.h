#ifndef RT_SOURCE_LOCATION_H
#define RT_SOURCE_LOCATION_H

#include <string>

namespace rt {

struct SourceLocation {
  std::string file;
  int line;
  int column;
  int discriminator;

  SourceLocation() : line(0), column(0), discriminator(0) {}
  SourceLocation(std::string f, int l, int c, int d = 0)
      : file(f), line(l), column(c), discriminator(d) {}

  bool operator<(const SourceLocation &other) const {
    if (file != other.file) return file < other.file;
    if (line != other.line) return line < other.line;
    if (column != other.column) return column < other.column;
    return discriminator < other.discriminator;
  }

  bool operator==(const SourceLocation &other) const {
    return file == other.file && line == other.line && column == other.column &&
           discriminator == other.discriminator;
  }
};

struct BaseSourceLocation {
  std::string file;
  int line;
  int column;

  BaseSourceLocation(const SourceLocation &loc)
      : file(loc.file), line(loc.line), column(loc.column) {}
  BaseSourceLocation(std::string f, int l, int c)
      : file(f), line(l), column(c) {}

  bool operator<(const BaseSourceLocation &other) const {
    if (file != other.file) return file < other.file;
    if (line != other.line) return line < other.line;
    return column < other.column;
  }
};

} // namespace rt

#endif
