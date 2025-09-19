# Changelog

All notable changes to this project will be documented in this file.

## [0.1.0] — initial milestone
### Added
- Block-level **sender**: assembles and sends N data + K parity frames; pacing via `--fps`; streaming via `--blocks`, `--inter-block-ms`.
- Block-level **receiver**: ingests frames into `RxBlockTable`, closes blocks by policy; `--expect-blocks` to stop after N blocks.
- **FEC core**: XOR (K=1) and GF(256) (K∈[2..4]) parity paths with deterministic tests.
- **Protocol**: base + parity subheaders, CRC32C over payload, encode/decode helpers.
- **Pipeline**: close policy (earliest of parity+all data, 60 ms cap, or `reorder_ms`), deterministic simulator helpers.
- **Transport**: Boost.Asio UDP; multicast strictness; sender `--mcast-ttl` and `--mcast-loopback`.
- **Metrics**: CSV per run with fixed schema (`schema_version,run_uuid,ts_ms,app,event,ip,port,bytes`).
- **Tests**: unit/integration and acceptance target (effective loss ≤ 0.2× raw loss) with quick/full modes.
- **CI**: GitHub Actions workflow (Windows), builds Boost.Program_options, builds Debug/Release, runs tests (quick).

### Docs
- `docs/DESIGN.md`, `docs/README.md`, `docs/TEST-MATRIX.md`.