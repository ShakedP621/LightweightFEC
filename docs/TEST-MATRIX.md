# TEST-MATRIX

This matrix tracks unit, integration, and acceptance coverage for LightweightFEC.

---

## Unit tests (deterministic)

**Protocol**
- Frame size computations, encode/decode (data + parity).
- CRC32C serialization and `verify_payload_crc`.
- Header roundtrip (`flags2` parity count packing/unpacking).

**Transport**
- Endpoint parser/formatter (IPv4 + port), multicast detection, octet access.

**FEC core**
- XOR (`K=1`) parity generation + single-erasure recovery.
- GF(256) (`K∈[2..4]`): parity rows `α^(j·d)`; solver recovers up to K erasures; partial parity availability.

**Pipeline**
- `BlockTracker` close logic:
  - `have_parity && have_all_data`
  - `age ≥ min(60 ms, 2×span)`
  - `age ≥ reorder_ms` (default 50 ms)
- `RxBlockTable` ingest/close/extract:
  - K=1: recover exactly one missing data.
  - K≥2: recover up to K missing with sufficient parity rows.

**Files (examples)**
- `crc32c_tests.cpp`, `frame_builder_tests.cpp`, `ip_tests.cpp`
- `rx_recover_k1_tests.cpp`, `rx_recover_gf256_tests.cpp`

---

## Integration tests

**Single-block e2e** (deterministic jitter)
- No loss: payload identity across TX→RX.
- Chosen losses:
  - `K=1`: exactly one data drop → recovered.
  - `K=2`: two data drops → recovered with two parity rows present.

**Multi-block stream**
- Bernoulli loss (p), uniform jitter `[0, J]`.
- Assemble blocks at 30 FPS, N=8; ingest into RX in arrival order.
- Compute:
  - `raw_loss = dropped_data / total_data`
  - `effective_loss = unrecovered_data / total_data`
- Assert acceptance (below).

**Files**
- `sim_e2e_tests.cpp`

---

## Acceptance (default N=8, K=1, fps=30, jitter≤50 ms)

Run for **p ∈ {1%, 3%, 5%}** with deterministic seeds.

- **Quick mode** (~15s): `LTFEC_ACCEPT_FULL=0` (default), small tolerance.
- **Full mode** (120s): `LTFEC_ACCEPT_FULL=1`, tight tolerance.

**Assertion**
effective_loss ≤ 0.2 × raw_loss (with small slack in quick mode)

yaml
Copy code

**File**
- `acceptance_tests.cpp`

---

## Manual checks

- **Unicast** sanity: loopback send/recv with `crc=ok`.
- **Multicast** single-host:
  - Receiver: `--listen 239.1.1.1:5555 --mcast-if <IFADDR>`
  - Sender: `--dest 239.1.1.1:5555 --mcast-if <IFADDR> --mcast-ttl 1 --mcast-loopback 1`
- Metrics CSV presence and schema header:
schema_version,run_uuid,ts_ms,app,event,ip,port,bytes

yaml
Copy code

---

## Skipped/disabled in CI

- Any test requiring external multicast routing.
- Full-length acceptance unless `LTFEC_ACCEPT_FULL=1`.