# Debug baseline validation

Pinned upstream source: `37551902752724327948a44db3e917e089598b37`.
The fork documentation/workflow changes do not modify that game source.

The `Fork Debug baseline` workflow is manually dispatched with an explicit
source commit. It builds the game plus upstream tests in Debug on Ubuntu 24.04
with GCC 14 and on native Windows with MSVC and upstream's pinned vcpkg baseline
`40f3c709db80acf154ac4b17a1f83c564ebd022e`. Compilation and vcpkg concurrency
are capped at two. CTest must discover tests; an empty selection fails.

The shareware test fixture is fetched from
`https://github.com/diasurgical/devilutionx-assets/releases/download/v2/spawn.mpq`.
SHA-256: `64427cd7c1ba904eaa2e0031c16a6b136d0ecef9abc888c5ff8344b459356e38`.
It stays in the ignored build directory. Test reports are the only uploaded
artifacts. No original full-game assets or playable fork packages are published.

Fork administrators have disabled the inherited multi-platform/release workflows
in the fork's Actions settings; their source definitions remain for reference.
This avoids launching every unrelated console/phone build or upstream release
packager while proving the baseline. Re-enable only deliberately adapted jobs.
Our dedicated workflow is dispatch-only and cannot publish a release.

## Linux reproduction

Install dependencies as in the dedicated workflow, including GCC/G++ 14. Then:

```sh
CC=gcc-14 CXX=g++-14 cmake -S . -B build/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DDEVILUTIONX_SYSTEM_BENCHMARK=OFF
curl --fail --location --retry 2 \
  https://github.com/diasurgical/devilutionx-assets/releases/download/v2/spawn.mpq \
  -o build/debug/spawn.mpq
echo '64427cd7c1ba904eaa2e0031c16a6b136d0ecef9abc888c5ff8344b459356e38  build/debug/spawn.mpq' | sha256sum --check
cmake --build build/debug --parallel 2
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ctest --test-dir build/debug --output-on-failure --no-tests=error
```

The source requires C++23, including `std::format` and `std::expected`.
GCC 12's system library is insufficient. Clang with a modern libc++ is an
alternative; compile GoogleTest/benchmark with the same library. When vendoring
GoogleTest, standalone tests including gmock headers must also see the matching
vendored googlemock include directory, rather than older system headers.

The original pinned source fails Clang23/libc++ at `Source/tables/textdat.h`:
it uses `std::vector` without its direct include. Fork commit `ad72f2915` adds
that include. The next compile exposed missing `<cstdlib>` for `std::getenv` in
`Source/platform/locale.cpp`; fork commit `df52384a2` adds it. Record original-source
and final fixed-source results separately. Clang23
also diagnoses benchmark1.8.5's `__COUNTER__` with its `-pedantic-errors`. The
local validation builds the game and all CTest executables explicitly, excluding
standalone performance benchmark targets; native GCC/MSVC CI still builds all
default targets. This does not constitute a performance benchmark.

## Windows reproduction

Use a native MSVC developer shell, Ninja, CMake and the pinned vcpkg checkout.
Set `VCPKG_ROOT` to that checkout and `VCPKG_MAX_CONCURRENCY=2`. Copy
`Packaging/windows/CMakePresets.json` to a temporary root `CMakePresets.json`,
configure `ninja-vcpkg-debug`, fetch and verify the same test fixture into
`build-ninja-vcpkg-debug`, then:

```powershell
cmake --build --preset ninja-vcpkg-debug --parallel 2
ctest --preset ninja-vcpkg-debug --no-tests=error
```

## Evidence boundary

Record actual heads, run URLs, results and limitations in NOTES.md. Passing
these tests establishes an upstream Debug baseline only. Shareware fixtures
do not prove full Diablo quests, local shared-screen co-op with nonblocking personal panels,
performance or physical controller/Deck behavior. Those remain separate gates.

## Vendored GoogleTest and sanitizer consistency

Clang23/libc++ Debug initially executed all584 cases with539 passes,43 failed
LeakSanitizer exits and2 upstream skips. The assertions passed in those43 cases;
that original suite still failed and its evidence is retained. The64-byte report
pointed at a live string in `OverridePaths`, not a missing vector destructor.

A focused archive load/unload/reload probe reproduces the failure when linked
with the same shared backend, test main and unsanitized vendored GoogleTest.
After reload, ASan reports both the live vector element and its string-pointer
field poisoned. libc++ template instantiations from the unsanitized test library
can override instrumented instantiations used by the shared backend: reusing
vector capacity then omits the annotation that makes the element addressable.
LeakSanitizer ignores the poisoned pointer and incorrectly reports its allocation
as unreachable. Debugger tracing separately confirms the vector/string destructors
run; leak checking precedes shared-backend destruction. Plain vector/DSO probes
without this mixed test-library configuration do not reproduce it.

Rebuilding only GoogleTest with matching address/undefined sanitizers leaves
the same archive probe's live element/pointer unpoisoned and its exit clean.
The unchanged original dungeon case also exits clean when relinked with that
library. The fork now applies its configured sanitizer flags to all four
vendored GoogleTest/GoogleMock targets, using the same Debug/Release option
expressions as game targets. Imported system/vcpkg test libraries are unchanged.
No leak suppression, `detect_leaks=0` validation or gameplay change is involved.
The debugger-only `detect_leaks=0` trace is not a sanitizer pass: LeakSanitizer
cannot run under ptrace. The complete normal CMake game/all56-test build and full local ASan/UBSan
CTest584 subsequently passed (582 passed,0 failures,2 upstream skips); a diagnostic
relink alone was not used as final-source evidence.

## Completed sanitizer-config baseline

Source/workflow `f53af5e5dd702cb5300d718947db6f7dded08d13` passed
[native run38189532088](https://github.com/jarvis-jhk/diablo-splitscreen/actions/runs/38189532088).
Both jobs and the workflow completed successfully. Actual source-output verification
and downloaded JUnit XML confirm Linux584 and Windows558 cases, zero failures and
only the two inherited VisualStore pagination skips on each platform. Artifact IDs,
ZIP sizes and SHA256 hashes are recorded in NOTES.md. Original and portability-source
runs are separate history. Supported baseline/sanitizer concerns are resolved;
full-game data, shared-screen/personal UI, hardware and performance remain unproven.
The next bounded experiment is [TWO-PLAYER-PROOF.md](TWO-PLAYER-PROOF.md).
