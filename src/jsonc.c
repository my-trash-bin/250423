#include "jsonc.h"

#ifdef __cplusplus

#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>

#else

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#endif

typedef bool err_t;

typedef struct arraybuffer {
  void *data;
  size_t element_size;
  size_t capacity;
  size_t length;
} arraybuffer;

static inline arraybuffer *arraybuffer_create(size_t element_size,
                                              size_t capacity) {
  arraybuffer *buffer = malloc(sizeof(arraybuffer));
  if (!buffer) {
    return NULL;
  }
  buffer->data = malloc(element_size * capacity);
  if (!buffer->data) {
    free(buffer);
    return NULL;
  }
  buffer->element_size = element_size;
  buffer->capacity = capacity;
  buffer->length = 0;
  return buffer;
}

static inline void arraybuffer_destroy(arraybuffer *buffer) {
  free(buffer->data);
  free(buffer);
}

static inline void *arraybuffer_get(arraybuffer *buffer, size_t index) {
  return (char *)buffer->data + index * buffer->element_size;
}

static inline void arraybuffer_set(arraybuffer *buffer, size_t index,
                                   const void *value) {
  memcpy(arraybuffer_get(buffer, index), value, buffer->element_size);
}

static inline err_t arraybuffer_push(arraybuffer *buffer, const void *value) {
  if (buffer->length == buffer->capacity) {
    const size_t new_capacity = buffer->capacity * 2;
    void *const new_data =
        realloc(buffer->data, new_capacity * buffer->element_size);
    if (!new_data) {
      return true;
    }
    buffer->data = new_data;
    buffer->capacity = new_capacity;
  }
  memcpy(arraybuffer_get(buffer, buffer->length), value, buffer->element_size);
  buffer->length++;
  return false;
}

static inline char *util_strdup(const char *str) {
  char *const result = malloc(strlen(str) + 1);
  if (!result) {
    return NULL;
  }
  strcpy(result, str);
  return result;
}

typedef enum token_type {
  TT_EOF,
  TT_LEFT_BRACE,
  TT_RIGHT_BRACE,
  TT_LEFT_BRACKET,
  TT_RIGHT_BRACKET,
  TT_COMMA,
  TT_COLON,
  TT_STRING,
  TT_NUMBER,
  TT_TRUE,
  TT_FALSE,
  TT_NULL,
} token_type;

typedef struct token {
  token_type type;
  union {
    char *string;
    double number;
  } value;
} token;

#define TS_ERROR -1
#define TS_DEFAULT 0
#define TS_KEYWORD_T 1
#define TS_KEYWORD_TR 2
#define TS_KEYWORD_TRU 3
#define TS_KEYWORD_F 4
#define TS_KEYWORD_FA 5
#define TS_KEYWORD_FAL 6
#define TS_KEYWORD_FALS 7
#define TS_KEYWORD_N 8
#define TS_KEYWORD_NU 9
#define TS_KEYWORD_NUL 10
#define TS_STRING_ANY 11
#define TS_STRING_BACKSLASH 12
// TODO: support unicode escape sequences and utf-8mb4 instead of \xXX
#define TS_STRING_X0 13
#define TS_STRING_X1 14
#define TS_NUMBER_SIGN 15
#define TS_NUMBER_ZERO 16
#define TS_NUMBER_INTEGER 17
#define TS_NUMBER_DOT 18
#define TS_NUMBER_FRACTION 19
#define TS_NUMBER_E 20
#define TS_NUMBER_E_SIGN 21
#define TS_NUMBER_E_DIGIT 22

typedef struct tokenizer_state_string {
  arraybuffer *stringbuilder;
  unsigned char x;
} tokenizer_state_string;

typedef struct tokenizer_state_number {
  double value;
  double current_digit;
  int sign;
  int exp;
  int exp_sign;
} tokenizer_state_number;

typedef union tokenizer_state_data {
  tokenizer_state_string string;
  tokenizer_state_number number;
} tokenizer_state_data;

typedef struct tokenizer_state {
  int state;
  tokenizer_state_data data;
} tokenizer_state;

typedef err_t (*tokenizer_state_function)(
    char c, arraybuffer *list, // arraybuffer of tokens
    tokenizer_state_data *data, tokenizer_state *out_next_state);

static double exponential(double n, int e) {
  while (e < 0) {
    e++;
    n /= 10;
  }
  while (e > 0) {
    e--;
    n *= 10;
  }
  return n;
}

static err_t add_number_token(arraybuffer *list,
                              tokenizer_state_number *state) {
  const double number =
      exponential(state->value * state->sign, state->exp * state->exp_sign);
  const token current = {.type = TT_NUMBER, .value.number = number};

  if (!arraybuffer_push(list, &current)) {
    return true;
  }
  return false;
}

static err_t add_string_token(arraybuffer *list,
                              /* always-consumed */
                              tokenizer_state_string *state) {
  char *const string = util_strdup(arraybuffer_get(state->stringbuilder, 0));
  err_t result = true;
  if (!string) {
    goto cleanup;
  }
  const token current = {.type = TT_STRING, .value.string = string};
  if (arraybuffer_push(list, &current)) {
    free(string);
    goto cleanup;
  }
  result = false;
  goto cleanup;

cleanup:
  arraybuffer_destroy(state->stringbuilder);
  state->stringbuilder = NULL;
  return result;
}

static err_t add_simple_token(arraybuffer *list, token_type type) {
  const token current = {.type = type};
  return arraybuffer_push(list, &current);
}

static err_t ts_default(char c, arraybuffer *list, tokenizer_state_data *data,
                        tokenizer_state *out_next_state) {
  (void)data;
  *out_next_state = (tokenizer_state){.state = TS_ERROR};
  if (!c || c == '[' || c == ']' || c == '{' || c == '}' || c == ':' ||
      c == ',') {
    *out_next_state = (tokenizer_state){.state = TS_DEFAULT};
    return ((!c && add_simple_token(list, TT_EOF)) ||
            (c == '[' && add_simple_token(list, TT_LEFT_BRACKET)) ||
            (c == ']' && add_simple_token(list, TT_RIGHT_BRACKET)) ||
            (c == '{' && add_simple_token(list, TT_LEFT_BRACE)) ||
            (c == '}' && add_simple_token(list, TT_RIGHT_BRACE)) ||
            (c == ':' && add_simple_token(list, TT_COLON)) ||
            (c == ',' && add_simple_token(list, TT_COMMA)));
  } else if (c == 't') {
    *out_next_state = (tokenizer_state){.state = TS_KEYWORD_T};
  } else if (c == 'f') {
    *out_next_state = (tokenizer_state){.state = TS_KEYWORD_F};
  } else if (c == 'n') {
    *out_next_state = (tokenizer_state){.state = TS_KEYWORD_N};
  } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
    *out_next_state = (tokenizer_state){.state = TS_DEFAULT};
  } else if (c == '-' || ('0' <= c && c <= '9')) {
    out_next_state->data.number.current_digit = 1;
    out_next_state->data.number.value = 0;
    out_next_state->data.number.sign = 1;
    out_next_state->data.number.exp = 0;
    out_next_state->data.number.exp_sign = 1;
    out_next_state->state = TS_NUMBER_INTEGER;
    if (c == '-') {
      out_next_state->data.number.sign = -1;
      out_next_state->state = TS_NUMBER_SIGN;
    } else if (c == '0') {
      out_next_state->state = TS_NUMBER_ZERO;
    } else {
      out_next_state->data.number.value = c - '0';
    }
  } else if (c == '"') {
    arraybuffer *const stringbuilder = arraybuffer_create(1, 128);
    if (!stringbuilder) {
      return true;
    }
    out_next_state->data.string.stringbuilder = stringbuilder;
    out_next_state->state = TS_STRING_ANY;
    return false;
  }
  return false;
}

static err_t ts_keyword_not_last(char c, arraybuffer *list,
                                 tokenizer_state_data *data,
                                 tokenizer_state *out_next_state, char expect,
                                 int next_state) {
  (void)list;
  (void)data;
  if (c == expect) {
    out_next_state->state = next_state;
  } else {
    out_next_state->state = TS_ERROR;
  }
  return false;
}

static err_t ts_keyword_last(char c, arraybuffer *list,
                             tokenizer_state_data *data,
                             tokenizer_state *out_next_state, char expect,
                             int next_state, token_type tt) {
  (void)data;
  if (c == expect) {
    if (add_simple_token(list, tt)) {
      return true;
    }
    out_next_state->state = next_state;
  } else {
    out_next_state->state = TS_ERROR;
  }
  return false;
}

static err_t ts_keyword_t(char c, arraybuffer *list, tokenizer_state_data *data,
                          tokenizer_state *out_next_state) {
  return ts_keyword_not_last(c, list, data, out_next_state, 'r', TS_KEYWORD_TR);
}

static err_t ts_keyword_tr(char c, arraybuffer *list,
                           tokenizer_state_data *data,
                           tokenizer_state *out_next_state) {
  return ts_keyword_not_last(c, list, data, out_next_state, 'u',
                             TS_KEYWORD_TRU);
}

static err_t ts_keyword_tru(char c, arraybuffer *list,
                            tokenizer_state_data *data,
                            tokenizer_state *out_next_state) {
  return ts_keyword_last(c, list, data, out_next_state, 'e', TS_DEFAULT,
                         TT_TRUE);
}

static err_t ts_keyword_f(char c, arraybuffer *list, tokenizer_state_data *data,
                          tokenizer_state *out_next_state) {
  return ts_keyword_not_last(c, list, data, out_next_state, 'a', TS_KEYWORD_FA);
}

static err_t ts_keyword_fa(char c, arraybuffer *list,
                           tokenizer_state_data *data,
                           tokenizer_state *out_next_state) {
  return ts_keyword_not_last(c, list, data, out_next_state, 'l',
                             TS_KEYWORD_FAL);
}

static err_t ts_keyword_fal(char c, arraybuffer *list,
                            tokenizer_state_data *data,
                            tokenizer_state *out_next_state) {
  return ts_keyword_not_last(c, list, data, out_next_state, 's',
                             TS_KEYWORD_FALS);
}

static err_t ts_keyword_fals(char c, arraybuffer *list,
                             tokenizer_state_data *data,
                             tokenizer_state *out_next_state) {
  return ts_keyword_last(c, list, data, out_next_state, 'e', TS_DEFAULT,
                         TT_FALSE);
}

static err_t ts_keyword_n(char c, arraybuffer *list, tokenizer_state_data *data,
                          tokenizer_state *out_next_state) {
  return ts_keyword_not_last(c, list, data, out_next_state, 'u', TS_KEYWORD_NU);
}

static err_t ts_keyword_nu(char c, arraybuffer *list,
                           tokenizer_state_data *data,
                           tokenizer_state *out_next_state) {
  return ts_keyword_not_last(c, list, data, out_next_state, 'l',
                             TS_KEYWORD_NUL);
}

static err_t ts_keyword_nul(char c, arraybuffer *list,
                            tokenizer_state_data *data,
                            tokenizer_state *out_next_state) {
  return ts_keyword_last(c, list, data, out_next_state, 'l', TS_DEFAULT,
                         TT_NULL);
}

static err_t ts_string_any(char c, arraybuffer *list,
                           tokenizer_state_data *data,
                           tokenizer_state *out_next_state) {
  (void)data;
  if (c == '"') {
    *out_next_state = (tokenizer_state){.state = TS_DEFAULT};
    return add_string_token(list, &data->string);
  } else if (c == '\\') {
    *out_next_state =
        (tokenizer_state){.state = TS_STRING_BACKSLASH, .data = *data};
    return false;
  } else if (iscntrl(c)) {
    *out_next_state = (tokenizer_state){.state = TS_ERROR};
    return false;
  }
  *out_next_state = (tokenizer_state){.state = TS_STRING_ANY, .data = *data};
  if (arraybuffer_push(data->string.stringbuilder, &c)) {
    arraybuffer_destroy(data->string.stringbuilder);
    return true;
  }
  return false;
}

static err_t ts_string_backslash(char c, arraybuffer *list,
                                 tokenizer_state_data *data,
                                 tokenizer_state *out_next_state) {
  (void)list;
  if (c == 'x') {
    *out_next_state = (tokenizer_state){.state = TS_STRING_X0, .data = *data};
    return false;
  }
  char next;
  if (c == '\"' || c == '\\' || c == '/') {
    next = c;
  } else if (c == 'b') {
    next = '\b';
  } else if (c == 'f') {
    next = '\f';
  } else if (c == 'n') {
    next = '\n';
  } else if (c == 'r') {
    next = '\r';
  } else if (c == 't') {
    next = '\t';
  } else {
    next = 0;
  }
  if (!next) {
    arraybuffer_destroy(data->string.stringbuilder);
    *out_next_state = (tokenizer_state){.state = TS_ERROR};
    return false;
  }
  if (arraybuffer_push(data->string.stringbuilder, &next)) {
    arraybuffer_destroy(data->string.stringbuilder);
    return true;
  }
  *out_next_state = (tokenizer_state){.state = TS_STRING_ANY, .data = *data};
  return false;
}

static unsigned char from_hex(char c) {
  if ('0' <= c && c <= '9')
    return (c - '0');
  if ('a' <= c && c <= 'f')
    return (c - 'a' + 10);
  if ('A' <= c && c <= 'F')
    return (c - 'A' + 10);
  return (-1);
}

static err_t ts_string_x0(char c, arraybuffer *list, tokenizer_state_data *data,
                          tokenizer_state *out_next_state) {
  (void)list;

  const unsigned char value = from_hex(c);
  if (value == (unsigned char)-1) {
    arraybuffer_destroy(data->string.stringbuilder);
    *out_next_state = (tokenizer_state){.state = TS_ERROR};
    return false;
  }
  *out_next_state = (tokenizer_state){.state = TS_STRING_X1, .data = *data};
  data->string.x = value;
  return false;
}

static err_t ts_string_x1(char c, arraybuffer *list, tokenizer_state_data *data,
                          tokenizer_state *out_next_state) {
  (void)list;

  const unsigned char value = from_hex(c);
  if (value == (unsigned char)-1) {
    arraybuffer_destroy(data->string.stringbuilder);
    *out_next_state = (tokenizer_state){.state = TS_ERROR};
    return false;
  }
  *out_next_state = (tokenizer_state){.state = TS_STRING_ANY, .data = *data};
  const int tmp = ((data->string.x) << CHAR_BIT) | value;
  if (arraybuffer_push(data->string.stringbuilder, &tmp)) {
    arraybuffer_destroy(data->string.stringbuilder);
    return true;
  }
  return false;
}

static err_t ts_number_sign(char c, arraybuffer *list,
                            tokenizer_state_data *data,
                            tokenizer_state *out_next_state) {
  (void)list;
  if ('0' <= c && c <= '9') {
    data->number.value = data->number.value * 10 + (c - '0');
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_INTEGER, .data = *data};
  } else {
    *out_next_state = (tokenizer_state){.state = TS_ERROR};
  }
  return false;
}

static err_t ts_number_zero(char c, arraybuffer *list,
                            tokenizer_state_data *data,
                            tokenizer_state *out_next_state) {

  if (c == '.') {
    *out_next_state = (tokenizer_state){.state = TS_NUMBER_DOT, .data = *data};
    return false;
  } else if (c == 'e' || c == 'E') {
    *out_next_state = (tokenizer_state){.state = TS_NUMBER_E, .data = *data};
    return false;
  }
  if ('0' <= c && c <= '9') {
    *out_next_state = (tokenizer_state){.state = TS_ERROR};
    return false;
  }
  if (add_number_token(list, &data->number)) {
    return true;
  }
  *out_next_state = (tokenizer_state){.state = TS_DEFAULT};
  return ts_default(c, list, data, out_next_state);
}

static err_t ts_number_integer(char c, arraybuffer *list,
                               tokenizer_state_data *data,
                               tokenizer_state *out_next_state) {
  if (c == '.') {
    *out_next_state = (tokenizer_state){.state = TS_NUMBER_DOT, .data = *data};
    return false;
  } else if (c == 'e' || c == 'E') {
    *out_next_state = (tokenizer_state){.state = TS_NUMBER_E, .data = *data};
    return false;
  }
  if ('0' <= c && c <= '9') {
    data->number.value = data->number.value * 10 + (c - '0');
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_INTEGER, .data = *data};
    return false;
  }
  if (add_number_token(list, &data->number)) {
    return true;
  }
  *out_next_state = (tokenizer_state){.state = TS_DEFAULT};
  return ts_default(c, list, data, out_next_state);
}

static err_t ts_number_dot(char c, arraybuffer *list,
                           tokenizer_state_data *data,
                           tokenizer_state *out_next_state) {
  (void)list;

  if ('0' <= c && c <= '9') {
    data->number.current_digit /= 10;
    data->number.value += data->number.current_digit * (c - '0');
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_FRACTION, .data = *data};
    return false;
  }
  *out_next_state = (tokenizer_state){.state = TS_ERROR};
  return false;
}

static err_t ts_number_fraction(char c, arraybuffer *list,
                                tokenizer_state_data *data,
                                tokenizer_state *out_next_state) {
  if (c == 'e' || c == 'E') {
    *out_next_state = (tokenizer_state){.state = TS_NUMBER_E, .data = *data};
    return false;
  }
  if ('0' <= c && c <= '9') {
    data->number.current_digit /= 10;
    data->number.value += data->number.current_digit * (c - '0');
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_FRACTION, .data = *data};
    return false;
  }
  if (add_number_token(list, &data->number)) {
    return true;
  }
  *out_next_state = (tokenizer_state){.state = TS_DEFAULT};
  return ts_default(c, list, data, out_next_state);
}

static err_t ts_number_e(char c, arraybuffer *list, tokenizer_state_data *data,
                         tokenizer_state *out_next_state) {
  (void)list;

  if (c == '+' || c == '-') {
    if (c == '-') {
      data->number.exp_sign = -1;
    }
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_E_SIGN, .data = *data};
    return false;
  }
  if ('0' <= c && c <= '9') {
    data->number.exp = data->number.exp * 10 + (c - '0');
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_E_DIGIT, .data = *data};
    return false;
  }
  *out_next_state = (tokenizer_state){.state = TS_ERROR};
  return false;
}

static err_t ts_number_e_sign(char c, arraybuffer *list,
                              tokenizer_state_data *data,
                              tokenizer_state *out_next_state) {
  (void)list;

  if ('0' <= c && c <= '9') {
    data->number.exp = data->number.exp * 10 + (c - '0');
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_E_DIGIT, .data = *data};
    return false;
  }
  *out_next_state = (tokenizer_state){.state = TS_ERROR};
  return false;
}

static err_t ts_number_e_digit(char c, arraybuffer *list,
                               tokenizer_state_data *data,
                               tokenizer_state *out_next_state) {
  if ('0' <= c && c <= '9') {
    data->number.exp = data->number.exp * 10 + (c - '0');
    *out_next_state =
        (tokenizer_state){.state = TS_NUMBER_E_DIGIT, .data = *data};
    return false;
  }
  if (add_number_token(list, &data->number)) {
    return true;
  }
  *out_next_state = (tokenizer_state){.state = TS_DEFAULT};
  return ts_default(c, list, data, out_next_state);
}

static const tokenizer_state_function state_functions[] = {
    ts_default,          ts_keyword_t,       ts_keyword_tr,
    ts_keyword_tru,      ts_keyword_f,       ts_keyword_fa,
    ts_keyword_fal,      ts_keyword_fals,    ts_keyword_n,
    ts_keyword_nu,       ts_keyword_nul,     ts_string_any,
    ts_string_backslash, ts_string_x0,       ts_string_x1,
    ts_number_sign,      ts_number_zero,     ts_number_integer,
    ts_number_dot,       ts_number_fraction, ts_number_e,
    ts_number_e_sign,    ts_number_e_digit,
};

static void tokenize_free(arraybuffer *tokens) {
  for (size_t i = 0; i < tokens->length; i++) {
    token *token = arraybuffer_get(tokens, i);
    if (token->type == TT_STRING) {
      free(token->value.string);
    }
  }
  arraybuffer_destroy(tokens);
}

static err_t tokenize(const char *str, arraybuffer **out) {
  tokenizer_state current_state = {.state = TS_DEFAULT};
  arraybuffer *tokens = arraybuffer_create(sizeof(token), 128);
  size_t i = -1;
  while (current_state.state != TS_ERROR && (i++ == (size_t)-1 || str[i - 1])) {
    if (state_functions[current_state.state](
            str[i], tokens, &current_state.data, &current_state)) {
      tokenize_free(tokens);
      return (true);
    }
  }
  if (current_state.state == TS_ERROR) {
    tokenize_free(tokens);
    *out = NULL;
    return (false);
  }
  *out = tokens;
  return (false);
}

static token token_get(arraybuffer *tokens, size_t index) {
  return *((token *)arraybuffer_get(tokens, index));
}

static err_t parse_array(arraybuffer *list, size_t *index, jsonc_array *out);
static err_t parse_object(arraybuffer *list, size_t *index, jsonc_object *out);
static err_t parse_value(arraybuffer *list, size_t *index, jsonc_value *out);

static bool parse_next_is_array(token_type type) {
  return type == TT_LEFT_BRACKET;
}

static bool parse_next_is_object(token_type type) {
  return type == TT_LEFT_BRACE;
}

static bool parse_next_is_value(token_type type) {
  return type == TT_NULL || type == TT_TRUE || type == TT_FALSE ||
         type == TT_NUMBER || type == TT_STRING || parse_next_is_array(type) ||
         parse_next_is_object(type);
}

static err_t parse_array(arraybuffer *list, size_t *index, jsonc_array *out) {
  // TODO: implement
  return false;
}

static err_t parse_object(arraybuffer *list, size_t *index, jsonc_object *out) {
  // TODO: implement
  return false;
}

static err_t parse_value(arraybuffer *list, size_t *index, jsonc_value *out) {
  if (!parse_next_is_value(token_get(list, *index).type)) {
    out->type = JSONC_VALUE_TYPE_ERROR;
    return false;
  } else if (parse_next_is_array(token_get(list, *index).type)) {
    return parse_array(list, index, out);
  } else if (parse_next_is_object(token_get(list, *index).type)) {
    return parse_object(list, index, out);
  } else {
    err_t result;

    result = false;
    if (token_get(list, *index).type == TT_NULL)
      out->type = JSONC_VALUE_TYPE_NULL;
    else if (token_get(list, *index).type == TT_TRUE ||
             token_get(list, *index).type == TT_FALSE) {
      out->type = JSONC_VALUE_TYPE_BOOLEAN;
      out->value.boolean = token_get(list, *index).type == TT_TRUE;
    } else if (token_get(list, *index).type == TT_NUMBER) {
      out->type = JSONC_VALUE_TYPE_NUMBER;
      out->value.number = token_get(list, *index).value.number;
    } else if (token_get(list, *index).type == TT_STRING) {
      out->type = JSONC_VALUE_TYPE_STRING;
      out->value.string = util_strdup(token_get(list, *index).value.string);
      result = !out->value.string;
    } else {
      return true;
    }
    (*index)++;
    return (result);
  }
}
