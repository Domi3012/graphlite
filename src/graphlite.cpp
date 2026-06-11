/**
 * @file graphlite.cpp
 * @brief Single Compilation Unit — biên dịch toàn bộ GraphLite library.
 *
 * File này là NGUỒN DUY NHẤT cần compile khi dùng compiled mode (default).
 * Nó define GRAPHLITE_IMPLEMENTATION rồi include umbrella header,
 * kích hoạt tất cả implementation blocks trong headers.
 *
 * Đây là pattern "Unity Build" — tương tự SQLite (sqlite3.c).
 */

#define GRAPHLITE_IMPLEMENTATION
#include <graphlite/graphlite.h>
