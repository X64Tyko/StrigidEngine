#pragma once

// TrinyxMinimal.h — Precompiled header for the Trinyx engine and game modules.
//
// Include policy:
//   Tier 1 — C++ stdlib headers that appear in 14+ translation units. These never
//             change and have no engine dependencies.
//   Tier 2 — Stable engine-core headers with 30+ includes and no active R&D churn.
//             Logger.h and Types.h qualify; reflection/ECS headers do not (they evolve).
//
// What does NOT belong here:
//   - Third-party headers (imgui, Jolt, SDL3) — editor/physics/platform are conditional.
//   - SchemaReflector, Registry, TrinyxJobs — active development, would invalidate PCH often.
//   - Any generated or user-authored headers.

// =============================================================================
// Tier 1 — C++ Standard Library
// =============================================================================

// Fundamental types & intrinsics
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>
#include <climits>
#include <cfloat>

// Containers
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <optional>
#include <variant>
#include <initializer_list>

// Algorithms & utilities
#include <algorithm>
#include <numeric>
#include <utility>
#include <functional>
#include <memory>
#include <type_traits>
#include <concepts>
#include <limits>

// Concurrency
#include <atomic>
#include <mutex>
#include <condition_variable>

// I/O
#include <fstream>
#include <sstream>
#include <iomanip>

// Time
#include <chrono>

// =============================================================================
// Tier 2 — Stable Engine Core (high include-count, rarely change)
// =============================================================================

// Logging macros and Logger singleton — depends only on stdlib, 53 TUs.
#include "Logger.h"

// Fundamental engine types: EngineMode, Vector3/Quat/Matrix4, SimFloat aliases,
// CacheTier, FieldWidth, ComponentTypeID, MAX_* constants — 32+ TUs.
#include "Types.h"
