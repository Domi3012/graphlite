/**
 * @file graphlite.h
 * @brief Umbrella header — Include file này duy nhất để sử dụng toàn bộ GraphLite.
 * @version 1.0
 *
 * @code
 * // Compiled mode (default):
 * #include <graphlite/graphlite.h>
 *
 * // Header-only mode:
 * #define GRAPHLITE_HEADER_ONLY
 * #include <graphlite/graphlite.h>
 * @endcode
 */

#pragma once

// --- Foundation (types, macros) ---
#include "types.h"

// --- Internal utilities ---
#include "internal/BinaryUtils.h"
#include "internal/MiniVector.h"
#include "internal/NumericHashMap.h"
#include "internal/StringPoolDictionary.h"

// --- Platform & Storage ---
#include "internal/Platform.h"
#include "internal/MmapFile.h"
#include "internal/NodeStore.h"
#include "internal/PageManager.h"

// --- Metadata ---
#include "Schema.h"

// --- Core API ---
#include "GraphDB.h"
#include "traversal.h"
