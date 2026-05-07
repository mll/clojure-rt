#ifndef RT_RANDOM_ID_H
#define RT_RANDOM_ID_H

#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace rt {

inline std::string generateRandomHex(size_t length = 16) {
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<uint64_t> dist;

  std::stringstream ss;
  ss << std::hex << std::setfill('0');

  while (ss.str().length() < length) {
    ss << std::setw(16) << dist(gen);
  }

  std::string result = ss.str();
  if (result.length() > length) {
    result = result.substr(0, length);
  }
  return result;
}

} // namespace rt

#endif // RT_RANDOM_ID_H
