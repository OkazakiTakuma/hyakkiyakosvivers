#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

/// <summary>プレイヤー攻撃レベルの有効値と変換規則を一元管理します。</summary>
namespace PlayerAttackLevel {

inline constexpr std::string_view kDefault = "1";
inline constexpr std::string_view kSuper = "super";
inline constexpr std::array<std::string_view, 6> kValues = {"1", "2", "3", "4", "5", kSuper};

inline bool IsValid(std::string_view value) {
	for (const std::string_view candidate : kValues) {
		if (candidate == value) return true;
	}
	return false;
}

inline bool IsSuper(std::string_view value) {
	return value == kSuper;
}

inline std::string Normalize(std::string_view value) {
	return IsValid(value) ? std::string(value) : std::string(kDefault);
}

inline int ToPowerRank(std::string_view value) {
	// superは通常Lv5の次として扱い、不正値は既定のLv1へ戻す。
	if (IsSuper(value)) return 6;
	if (value.size() == 1 && value.front() >= '1' && value.front() <= '5') return value.front() - '0';
	return 1;
}

inline std::vector<std::string> GetValues() {
	std::vector<std::string> values;
	values.reserve(kValues.size());
	for (const std::string_view value : kValues) values.emplace_back(value);
	return values;
}

} // namespace PlayerAttackLevel
