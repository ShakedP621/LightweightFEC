#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/fec_core/xor_parity.h>
#include <algorithm>
#include <cstring>

namespace ltfec::pipeline {
    using namespace ltfec::protocol;

    bool TxBlockAssembler::assemble_block(const std::vector<std::span<const std::byte>>& data_payloads,
        std::vector<std::vector<std::byte>>& out_frames) noexcept
    {
        // Validate N
        if (data_payloads.size() != cfg_.N) return false;
        if (cfg_.N == 0) return false;

        const std::size_t L = data_payloads[0].size();
        if (L == 0) return false;
        if (cfg_.max_payload_len && L > cfg_.max_payload_len) return false;

        // Enforce equal length
        for (std::size_t i = 1; i < data_payloads.size(); ++i) {
            if (data_payloads[i].size() != L) return false;
        }

        // Prepare parity buffers
        const std::size_t K = cfg_.K;
        std::vector<std::vector<std::byte>> parity(K);
        for (std::size_t j = 0; j < K; ++j) parity[j].assign(L, std::byte{ 0 });

        // Build raw pointers for encoder
        std::vector<const std::byte*> data_ptrs(cfg_.N);
        for (std::size_t i = 0; i < cfg_.N; ++i) data_ptrs[i] = data_payloads[i].data();
        std::vector<std::byte*> parity_ptrs(cfg_.K);
        for (std::size_t j = 0; j < cfg_.K; ++j) parity_ptrs[j] = parity[j].data();

        const std::uint8_t scheme = enc_.encode(std::span<const std::byte* const>(data_ptrs.data(), data_ptrs.size()),
            L,
            std::span<std::byte*>(parity_ptrs.data(), parity_ptrs.size()));

        // Prepare output vector
        out_frames.clear();
        out_frames.resize(cfg_.N + cfg_.K);

        const std::uint16_t N = cfg_.N;
        const std::uint16_t K16 = cfg_.K;
        const std::uint32_t gen = next_gen_id_++;

        // Emit DATA frames
        for (std::uint16_t i = 0; i < N; ++i) {
            BaseHeader h{};
            h.version = k_protocol_version;
            h.flags1 = 0;
            h.flags2 = flags2_pack_parity_count_minus_one(K16);
            h.fec_gen_id = gen;
            h.seq_in_block = i;          // data index
            h.data_count = N;
            h.parity_count = K16;
            h.payload_len = static_cast<std::uint16_t>(L);

            auto& f = out_frames[i];
            f.resize(encoded_size(L, /*parity*/false));
            const bool ok = encode_data_frame(std::span<std::byte>(f.data(), f.size()),
                h, data_payloads[i]);
            if (!ok) return false;
        }

        // Emit PARITY frames (if any)
        for (std::uint16_t j = 0; j < K16; ++j) {
            BaseHeader h{};
            h.version = k_protocol_version;
            h.flags1 = 0;
            h.flags2 = flags2_pack_parity_count_minus_one(K16);
            h.fec_gen_id = gen;
            h.seq_in_block = static_cast<std::uint16_t>(N + j); // mark as parity
            h.data_count = N;
            h.parity_count = K16;
            h.payload_len = static_cast<std::uint16_t>(L);

            ParitySubheader ps{};
            ps.fec_scheme_id = scheme;
            ps.fec_parity_index = static_cast<std::uint8_t>(j);

            auto& f = out_frames[N + j];
            f.resize(encoded_size(L, /*parity*/true));
            const bool ok = encode_parity_frame(std::span<std::byte>(f.data(), f.size()),
                h, ps,
                std::span<const std::byte>(parity[j].data(), parity[j].size()));
            if (!ok) return false;
        }

        return true;
    }

} // namespace ltfec::pipeline