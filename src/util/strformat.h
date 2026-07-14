#ifndef UTIL_STRFORMAT_H
#define UTIL_STRFORMAT_H

#include <stdarg.h>

typedef void strformat_cb_t (char c);

void vstrformat(strformat_cb_t cb, const char *fmt, va_list args);

#endif // UTIL_STRFORMAT_H
