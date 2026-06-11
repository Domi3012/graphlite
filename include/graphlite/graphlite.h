/**
 * @file graphlite.h
 * @brief Umbrella header — Include duy nhất file này để sử dụng toàn bộ GraphLite.
 * @version 1.0
 * 
 * @code
 * #include <graphlite/graphlite.h>
 * 
 * int main() {
 *     graphlite::GraphDB db("./my_graph");
 *     // ...
 * }
 * @endcode
 */

#pragma once

#include "types.h"
#include "Schema.h"
#include "GraphDB.h"
#include "traversal.h"
