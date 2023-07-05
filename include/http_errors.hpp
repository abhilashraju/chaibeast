#pragma once
#include <exception>
#include <stdexcept>
#include <type_traits>
namespace chai {
struct file_not_found : std::runtime_error {
  file_not_found(const std::string &error)
      : std::runtime_error("File Not Found:" + error) {}
};
} // namespace chai