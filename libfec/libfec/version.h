#pragma once
#include <string_view>

namespace ltfec {
	inline constexpr int kVersionMajor = 0;
	inline constexpr int kVersionMinor = 1;
	inline constexpr int kVersionPatch = 0;
	inline constexpr std::string_view kVersionString = "0.1.0";

	std::string_view version();
}  // namespace ltfec