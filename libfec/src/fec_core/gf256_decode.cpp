#include <ltfec/fec_core/gf256_decode.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace ltfec::fec_core {

    namespace {
        // GF(256) with primitive polynomial 0x11D, generator α=2.
        static constexpr std::uint16_t kPoly = 0x11D;

        struct Tables {
            std::array<std::uint8_t, 512> exp{};
            std::array<std::uint8_t, 256> log{};
            bool inited{ false };
        };

        static Tables& T() {
            static Tables t;
            if (!t.inited) {
                std::uint16_t x = 1;
                for (int i = 0; i < 255; ++i) {
                    t.exp[i] = static_cast<std::uint8_t>(x);
                    t.log[t.exp[i]] = static_cast<std::uint8_t>(i);
                    x <<= 1;
                    if (x & 0x100) x ^= kPoly;
                }
                for (int i = 255; i < 512; ++i) t.exp[i] = t.exp[i - 255];
                t.inited = true;
            }
            return t;
        }
        static inline std::uint8_t gf_add(std::uint8_t a, std::uint8_t b) { return a ^ b; }
        static inline std::uint8_t gf_mul(std::uint8_t a, std::uint8_t b) {
            if (!a || !b) return 0;
            auto& tb = T();
            return tb.exp[tb.log[a] + tb.log[b]];
        }
        static inline std::uint8_t gf_inv(std::uint8_t a) {
            auto& tb = T();
            // a^{-1} = α^{255 - log(a)}
            return tb.exp[255 - tb.log[a]];
        }
        static inline std::uint8_t gf_pow_alpha(unsigned e) {
            auto& tb = T();
            return tb.exp[e % 255];
        }
    } // namespace

    // Solve A x = b over GF(256), A is m×m, b is m×1; in-place Gauss-Jordan.
    // Returns false if singular (shouldn't happen with proper Vandermonde rows).
    static bool solve_gf256(std::vector<std::uint8_t>& A, std::vector<std::uint8_t>& b, int m) {
        auto idx = [&](int r, int c) { return r * m + c; };

        for (int col = 0, row = 0; col < m && row < m; ++col, ++row) {
            // Find pivot
            int piv = row;
            while (piv < m && A[idx(piv, col)] == 0) ++piv;
            if (piv == m) return false; // singular
            if (piv != row) {
                for (int c = col; c < m; ++c) std::swap(A[idx(row, c)], A[idx(piv, c)]);
                std::swap(b[row], b[piv]);
            }

            // Scale pivot row
            std::uint8_t inv = gf_inv(A[idx(row, col)]);
            for (int c = col; c < m; ++c) A[idx(row, c)] = gf_mul(A[idx(row, c)], inv);
            b[row] = gf_mul(b[row], inv);

            // Eliminate other rows
            for (int r = 0; r < m; ++r) if (r != row) {
                std::uint8_t f = A[idx(r, col)];
                if (!f) continue;
                for (int c = col; c < m; ++c)
                    A[idx(r, c)] = gf_add(A[idx(r, c)], gf_mul(f, A[idx(row, c)]));
                b[r] = gf_add(b[r], gf_mul(f, b[row]));
            }
        }
        return true;
    }

    bool gf256_recover_erasures_vandermonde(std::span<const std::byte* const> data_ptrs,
        std::span<const std::byte* const> parity_ptrs,
        std::size_t frame_len,
        std::span<const std::uint16_t> missing_indices,
        std::span<std::byte*> out_recovered) noexcept
    {
        const int m = static_cast<int>(missing_indices.size());
        if (m == 0) return true;
        if (m > 4) return false;
        if (out_recovered.size() != static_cast<size_t>(m)) return false;

        // Collect the first m available parity rows (j values)
        std::vector<int> rows;
        rows.reserve(m);
        for (int j = 0; j < static_cast<int>(parity_ptrs.size()); ++j) {
            if (parity_ptrs[j] != nullptr) rows.push_back(j);
            if (static_cast<int>(rows.size()) == m) break;
        }
        if (static_cast<int>(rows.size()) < m) return false; // insufficient equations

        // Precompute known-data contributions per row (for RHS)
        const std::size_t N = data_ptrs.size();

        // For each byte position, build A (m×m) and b (m) then solve.
        for (std::size_t i = 0; i < frame_len; ++i) {
            std::vector<std::uint8_t> A(m * m, 0u);
            std::vector<std::uint8_t> b(m, 0u);

            for (int r = 0; r < m; ++r) {
                const int j = rows[r];
                // Start with parity byte
                std::uint8_t rhs = static_cast<std::uint8_t>(parity_ptrs[j][i]);

                // Subtract known-data contributions: rhs -= Σ_known α^(j*d) * data[d][i]
                for (std::size_t d = 0; d < N; ++d) {
                    if (data_ptrs[d] == nullptr) continue; // missing -> unknown variable
                    const std::uint8_t coef = gf_pow_alpha(static_cast<unsigned>(j * static_cast<int>(d)));
                    rhs = gf_add(rhs, gf_mul(coef, static_cast<std::uint8_t>(data_ptrs[d][i])));
                }
                b[r] = rhs;

                // Fill matrix columns for unknowns (missing indices)
                for (int c = 0; c < m; ++c) {
                    const int d = static_cast<int>(missing_indices[c]);
                    A[r * m + c] = gf_pow_alpha(static_cast<unsigned>(j * d));
                }
            }

            if (!solve_gf256(A, b, m)) return false;

            // b now holds the solution vector x; write to outputs
            for (int c = 0; c < m; ++c) {
                out_recovered[c][i] = static_cast<std::byte>(b[c]);
            }
        }

        return true;
    }

} // namespace ltfec::fec_core