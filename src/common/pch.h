#ifndef PCH_H
#define PCH_H

// --- C++ 标准库 ---
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <memory>
#include <cmath>
#include <set>
#include <unordered_map>
#include <cctype>
#include <optional>
#include <future>
#include <functional>

// --- 第三方库 ---
#include <nlohmann/json.hpp>
// 注意: stb 库通常通过在 .cpp 文件中定义宏来包含实现，
// 因此不建议将 <stb/stb_image.h> 等放入PCH，以避免多重定义问题。

// --- 项目内常用头文件 ---
// "common_types.h" 被高频使用，且非常稳定，是PCH的理想选择。
#include "common_types.h"

#endif //PCH_H