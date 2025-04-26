#ifndef JSONC_H
#define JSONC_H

#ifdef __cplusplus
#include <cstddef>
extern "C" {
#else
#include <stdbool.h>
#include <stddef.h>
#endif

typedef enum jsonc_value_type {
  JSONC_VALUE_TYPE_ERROR,
  JSONC_VALUE_TYPE_NULL,
  JSONC_VALUE_TYPE_BOOLEAN,
  JSONC_VALUE_TYPE_NUMBER,
  JSONC_VALUE_TYPE_STRING,
  JSONC_VALUE_TYPE_ARRAY,
  JSONC_VALUE_TYPE_OBJECT,
} jsonc_value_type;

typedef struct jsonc_object jsonc_object;
typedef struct jsonc_array jsonc_array;

typedef struct jsonc_value {
  jsonc_value_type type;
  union {
    bool boolean;
    double number;
    char *string;
    jsonc_object *object;
    jsonc_array *array;
  } value;
} jsonc_value;

struct jsonc_array {
  jsonc_value **values;
  size_t count;
};

typedef struct jsonc_object_entry {
  const char *key;
  jsonc_value *value;
} jsonc_object_entry;

struct jsonc_object {
  jsonc_object_entry *entries;
  size_t count;
};

jsonc_value *jsonc_parse(const char *source);
void jsonc_free(jsonc_value *value);

#ifdef __cplusplus
}
#endif

#endif
