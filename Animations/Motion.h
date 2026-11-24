#pragma once
#include <algorithm>
#include <cmath>

// Durées (ms) et courbes communes à toutes les animations.
namespace Motion {
constexpr int Fast = 180;
constexpr int Normal = 280;
constexpr int Long = 650;
constexpr int Frame = 16;

inline double part(double t, double from, double to) { return std::clamp((t - from) / (to - from), 0.0, 1.0); }
inline double soft(double v) { v = std::clamp(v, 0.0, 1.0); return 1 - std::pow(1 - v, 3); }
inline double both(double v) { v = std::clamp(v, 0.0, 1.0); return v * v * (3 - 2 * v); }
}
