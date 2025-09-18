#pragma once
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace ltfec::util {

	// One-shot CRC32C (Castagnoli) over a byte span.
	uint32_t crc32c(std::span<const std::byte> data) noexcept;
	// Convenience overloads:
	uint32_t crc32c(std::string_view s) noexcept;
	uint32_t crc32c(const void* data, std::size_t len) noexcept;

	// Incremental API (init → update* → finish)
	inline constexpr uint32_t crc32c_init() noexcept { return 0xFFFF'FFFFu; }
	uint32_t crc32c_update(uint32_t state, std::span<const std::byte> data) noexcept;
	inline constexpr uint32_t crc32c_finish(uint32_t state) noexcept { return state ^ 0xFFFF'FFFFu; }

} // namespace ltfec::util