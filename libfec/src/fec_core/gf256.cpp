#include <ltfec/fec_core/gf256.h>
#include <array>
#include <cstdint>
#include <cstring>

namespace ltfec::fec_core {

    namespace {

        // GF(256) with primitive polynomial x^8 + x^4 + x^3 + x^2 + 1 (0x11D), generator α=2.
        static constexpr std::uint16_t kPoly = 0x11D;

        // exp[i] = α^i for i ∈ [0..510]; log[x] = i such that α^i = x (for x != 0).
        // We duplicate the exp table to avoid modulo in hot path.
        struct GfTables {
            std::array<std::uint8_t, 512> exp{};
            std::array<std::uint8_t, 256> log{}; // log[0] unused
            bool inited{ false };
        };

        static GfTables& tables() {
            static GfTables T;
            if (!T.inited) {
                // Build exp/log
                std::uint16_t x = 1;
                for (int i = 0; i < 255; ++i) {
                    T.exp[i] = static_cast<std::uint8_t>(x);
                    T.log[T.exp[i]] = static_cast<std::uint8_t>(i);
                    x <<= 1;
                    if (x & 0x100) x ^= kPoly;
                }
                // Duplicate to avoid modulo
                for (int i = 255; i < 512; ++i) {
                    T.exp[i] = T.exp[i - 255];
                }
                T.inited = true;
            }
            return T;
        }

        static inline std::uint8_t gf_mul(std::uint8_t a, std::uint8_t b) {
            if (a == 0 || b == 0) return 0;
            const auto& T = tables();
            // log(a*b) = log(a) + log(b)  (mod 255)
            const int sum = T.log[a] + T.log[b];
            return T.exp[sum]; // sum ∈ [0..508], exp[] duplicated
        }

        static inline std::uint8_t gf_pow_alpha(unsigned e) {
            const auto& T = tables();
            // α^e where e mod 255
            return T.exp[e % 255];
        }

    } // namespace

    void gf256_encode(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<std::byte*> parity_frames /* K in [2..4] */) noexcept
    {
        if (frame_len == 0 || data_frames.empty() || parity_frames.empty()) return;

        const std::size_t N = data_frames.size();
        const std::size_t K = parity_frames.size();
        if (K < 2 || K > 4) {
            // Respect design hint: optional GF(256) parity for K ∈ [2..4]; ignore otherwise.
            return;
        }

        // For each parity row j, parity_row = Σ_d ( α^(j*d) * data_d )
        for (std::size_t j = 0; j < K; ++j) {
            std::byte* out = parity_frames[j];
            if (!out) continue;

            // Zero-init output row
            std::memset(out, 0, frame_len);

            for (std::size_t d = 0; d < N; ++d) {
                const std::byte* src = data_frames[d];
                if (!src) continue; // tolerate null (though encoder expects non-null)
                const std::uint8_t coef = gf_pow_alpha(static_cast<unsigned>(j * d));
                if (coef == 0) continue; // α^e never 0 for finite e, but guard anyway

                for (std::size_t i = 0; i < frame_len; ++i) {
                    const std::uint8_t s = static_cast<std::uint8_t>(src[i]);
                    const std::uint8_t cur = static_cast<std::uint8_t>(out[i]);
                    out[i] = static_cast<std::byte>(cur ^ gf_mul(coef, s));
                }
            }
        }
    }

} // namespace ltfec::fec_core