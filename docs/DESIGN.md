# LightweightFEC — DESIGN (Source of Truth)

This document is the **locked design** used by code, tests, and CI. Any change requires bumping `schema_version` (metrics) and corresponding updates.

---

## Goals

- Low-latency Forward Error Correction (FEC) for real-time UDP streams (e.g., 30 FPS).
- Simple, deterministic framing and recovery with small code footprint.
- Windows-first, MSVC C++20. 1 UDP datagram == 1 frame.

---

## Framing & MTU constraints

- **One frame per UDP datagram** (no IP fragmentation).
- Typical safe payload budget: **≤ ~1200–1300 bytes** after headers and CRC.
- Each frame includes a **base header**; parity frames also include a **parity subheader**.
- **CRC32C** is computed **over the payload only** and carried with the frame for verification.

---

## Headers

### Base header (always present)

| Field          | Type | Notes                                      |
|----------------|------|--------------------------------------------|
| `version`      | u8   | Protocol version (current = 1)             |
| `flags1`       | u8   | Reserved                                   |
| `flags2`       | u8   | Carries `parity_count_minus_one` (low bits)|
| `fec_gen_id`   | u32  | Block/generation id                        |
| `seq_in_block` | u16  | 0..N-1 for data frames                     |
| `data_count`   | u16  | N (data per block)                         |
| `parity_count` | u16  | K (parity per block)                       |
| `payload_len`  | u16  | Payload bytes                              |

- `flags2`: encoder writes `parity_count_minus_one = (K==0?0:K-1)`; decoder derives K.

### Parity subheader (present only on parity frames)

| Field              | Type | Notes                                   |
|--------------------|------|-----------------------------------------|
| `fec_scheme_id`    | u8   | 0 = XOR (K=1), 1 = GF(256) (K∈[2..4])   |
| `fec_parity_index` | u8   | j in 0..K-1                             |
| `fec_gen_id`       | u32  | Duplicate of base for clarity           |

### CRC

- `crc32c(payload)` is serialized within the frame.
- On decode: `verify_payload_crc(payload, crc32c_in_frame)`.

---

## FEC schemes

### Block layout

- Each block has **N data** frames and **K parity** frames (N≥1, 0≤K≤4).
- Defaults: **N=8**, **K=1** (XOR baseline), **fps=30**.

### XOR (K=1)

- Parity is the XOR of all N data payloads (byte-wise).
- Recovers **exactly one** missing data payload when the other N−1 data + parity are present.

### GF(256) (K ∈ [2..4])

- Field: GF(2⁸) with primitive polynomial **0x11D**, generator **α = 2**.
- Parity rows:  
  `parity[j] = Σ_d α^(j·d) · data[d]` for j = 0..K−1, d = 0..N−1.
- Decoder: Constructs a Vandermonde system over GF(256) for unknown (missing) data and solves via Gauss-Jordan; recovers up to **K** erasures provided at least as many independent parity rows are present.

---

## RX close policy (block termination)

A receive block **closes at the earliest of**:

1. `have_parity && have_all_data` (fast path if nothing lost), or  
2. `age ≥ min(60 ms, 2 × block_span_ms)`, or  
3. `age ≥ reorder_ms` (**default 50 ms**).

Where:
- `block_span_ms = ceil(N / fps) × 1000 / fps`. For N=8, fps=30 → ~267 ms; thus rule (2) reduces to **60 ms** cap.
- Tests often set `reorder_ms = 200` so rule (2) dominates.

On close, the RX emits `RxClosedBlock`:
- `data[0..N-1]` (payloads; recovered or received),
- `was_recovered[0..N-1]` (bools),
- `N, K, payload_len, gen`.

---

## Simulator

- **Loss models**: Bernoulli (p) and **Gilbert–Elliott** (planned/optional).
- **Jitter**: uniform **[0, J] ms**.
- Deterministic RNG (**XorShift32**) for repeatability in tests/CI.

---

## Windows multicast

- **Sender** requires `--mcast-if` when destination is multicast; fails fast if missing.
- **Receiver** requires `--mcast-if` when listening on multicast; joins group on the given interface.
- Sender options:
  - `--mcast-ttl` (0..255), default 1.
  - `--mcast-loopback` (0/1) for single-host testing.

---

## Metrics

- **CSV per run + console summary**.
- Fixed schema (stable column order):  
  `schema_version,run_uuid,ts_ms,app,event,ip,port,bytes`
- `schema_version` starts at **1**; bump if columns/semantics change.

---

## Acceptance target

With **defaults** (N=8, K=1, 30 FPS, jitter ≤ 50 ms), over **120 s** runs and for **p ∈ {1%, 3%, 5%}**:
- **effective_loss ≤ 0.2 × raw_loss** (with tiny slack allowed in quick mode for CI).

**Definitions:**
- `raw_loss` = dropped **data** frames / total data frames.
- `effective_loss` = unrecovered data frames after RX close / total data frames.

---

## Code organization (high-level)

/libfec
include/ltfec/
fec_core/ # XOR, GF(256) encode/solve
pipeline/ # TX assembly, RX tables, close policy, trackers
protocol/ # headers, (en/de)coding, CRC32C
transport/ # endpoint parse, Asio UDP
sim/ # RNG, loss/jitter helpers
metrics/ # CSV writer, schema
util/ # UUIDs, helpers
src/ # implementations
/apps
fec_sender/ # CLI, single-frame demo send (block-level later)
fec_receiver/ # CLI, single-frame receive + decode/CRC
/tests # Boost.Test unit/integration/acceptance