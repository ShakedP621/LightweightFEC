[![CI](https://github.com/<OWNER>/<REPO>/actions/workflows/ci.yml/badge.svg)](https://github.com/<OWNER>/<REPO>/actions/workflows/ci.yml)
[![tag: v0.1.0](https://img.shields.io/badge/tag-v0.1.0-blue.svg)](https://github.com/<OWNER>/<REPO>/releases/tag/v0.1.0)
# LightweightFEC

Windows x64 C++20 MSVC solution providing low-latency FEC over UDP with a clean, testable design.

## Prerequisites

- **Windows 10+**
- **Visual Studio 2022** (Insider/Preview OK), workload: *Desktop development with C++*
- **Boost 1.89.0**
  - Headers and Program_options library
  - Set environment variables (user scope):
    - `BOOST_INCLUDEDIR=C:\local\boost_1_89_0`
    - `BOOST_LIBRARYDIR=C:\local\boost_1_89_0\lib64-msvc-14.3` *(adjust if your folder differs)*
  - Ensure the property sheet **`C:\dev\Boost.props`** exists and is attached (per-project via Property Manager). It adds:
    - `AdditionalIncludeDirectories=$(BOOST_INCLUDEDIR)`
    - `AdditionalLibraryDirectories=$(BOOST_LIBRARYDIR)`

## Build

Open `LightweightFEC.sln` and build:
- Configurations: **x64 Debug / Release**
- Toolset: MSVC, `/std:c++20`, `/W4`, `/permissive-`, `/MD[d]`
- Macros: `NOMINMAX;WIN32_LEAN_AND_MEAN;_WIN32_WINNT=0x0A00`

Or from a Developer PowerShell:
msbuild LightweightFEC.sln /m /p:Configuration=Debug /p:Platform=x64

## Run (unicast)

**Receiver**
x64\Debug\fec_receiver.exe --listen 0.0.0.0:5555

markdown
Copy code

**Sender**
x64\Debug\fec_sender.exe --dest 127.0.0.1:5555 --msg "hello" --N 8 --K 1

markdown
Copy code

## Run (multicast, same machine)

1) Find your local IPv4 (not 127.0.0.1), e.g., `192.168.1.23`.
2) **Receiver**
x64\Debug\fec_receiver.exe --listen 239.1.1.1:5555 --mcast-if 192.168.1.23

markdown
Copy code
3) **Sender**
x64\Debug\fec_sender.exe --dest 239.1.1.1:5555 --mcast-if 192.168.1.23 --mcast-ttl 1 --mcast-loopback 1 --msg "mcast hello" --N 8 --K 1

csharp
Copy code

## Metrics

Every run writes a CSV under `metrics/` with header:
schema_version,run_uuid,ts_ms,app,event,ip,port,bytes

markdown
Copy code
A brief summary footer is appended; CSV files are ignored by git.

## Tests

- Unit + integration: Boost.Test executable at `x64\Debug\tests.exe`.
- Quick run:
x64\Debug\tests.exe

markdown
Copy code
- Full acceptance (120s) enables strict loss targets:
  - Set environment: `LTFEC_ACCEPT_FULL=1`
  - Run `tests.exe`

## Acceptance Target

With defaults (N=8, K=1, 30 FPS, jitter ≤ 50 ms), **effective loss ≤ 0.2 × raw loss** for p ∈ {1%, 3%, 5%} over 120 s (small slack allowed in quick mode).

## CI

GitHub Actions workflow at `.github/workflows/ci.yml`:
- Builds Boost.Program_options on the runner (cached).
- Creates `C:\dev\Boost.props`.
- Builds Debug/Release with MSBuild.
- Runs tests (quick mode), skipping network-only scenarios.

## Layout

/apps
fec_sender/ # CLI, single-frame send demo (N,K settable)
/ fec_receiver/ # CLI, single-frame receive + decode/CRC
/libfec # static library: protocol, fec_core, pipeline, transport, sim, metrics, util
/tests # Boost.Test (unit, e2e, acceptance)
/docs # DESIGN.md, README.md, TEST-MATRIX.md
/.github/workflows/ci.yml

css
Copy code

See **docs/DESIGN.md** for full protocol details.