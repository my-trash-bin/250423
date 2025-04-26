#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "jsonc.h"

static std::string read_file(const char *path) {
  std::ifstream t(path);
  std::stringstream buffer;
  buffer << t.rdbuf();
  return buffer.str();
}

static void print_array(const jsonc_array &array);
static void print_object(const jsonc_object &object);
static void print_value(const jsonc_value &value);

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <jsonc file>\n", argv[0]);
    return 0;
  }
  const std::string source = read_file(argv[1]);
  jsonc_value value;
  bool is_error;
  if (jsonc_parse(source.c_str(), &value, &is_error)) {
    return 1;
  }
  if (is_error) {
    std::cout << "Error" << std::endl;
    return 0;
  }
  print_value(value);
  std::cout << std::endl;
  jsonc_free(value);
  return 0;
}

static void print_array(const jsonc_array &array) {
  std::cout << "[";
  for (size_t i = 0; i < array.count; i++) {
    print_value(array.values[i]);
    if (i < array.count - 1) {
      std::cout << ",";
    }
  }
  std::cout << "]";
}

static void print_object(const jsonc_object &object) {
  std::cout << "{";
  for (size_t i = 0; i < object.count; i++) {
    std::cout << object.entries[i].key << ":";
    print_value(object.entries[i].value);
    if (i <object.count - 1) {
      std::cout << ",";
    }
  }
  std::cout << "}";
}

static void print_value(const jsonc_value &value) {
  switch (value.type) {
    case JSONC_VALUE_TYPE_NULL:
      std::cout << "null";
      break;
    case JSONC_VALUE_TYPE_BOOLEAN:
      std::cout << (value.value.boolean ? "true" : "false");
      break;
    case JSONC_VALUE_TYPE_NUMBER:
      std::cout << value.value.number;
      break;
    case JSONC_VALUE_TYPE_STRING:
      std::cout << "\"" << value.value.string << "\"";
      break;
    case JSONC_VALUE_TYPE_ARRAY:
      print_array(value.value.array);
      break;
    case JSONC_VALUE_TYPE_OBJECT:
      print_object(value.value.object);
      break;
  }
}
