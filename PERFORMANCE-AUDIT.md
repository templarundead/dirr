# Performance audit findings (F1–F11)

**Reviewed revision:** `0495bf12094b059ad3cb08fd100ebdc065d411d5`

**Review type:** source inspection plus targeted local benchmarks. The AArch64 comparison below is specific to the tested host, build, and fixture; it is not a generalized performance guarantee.

**Scope:** findings F1–F11 from the supplied audit. F12/PGO is intentionally out of scope at the user's request.

## Summary

| Finding | Assessment |
| --- | --- |
| F1 | Confirmed structurally and optimized in this draft; the targeted AArch64 `-w` benchmark measured 7.46% lower mean wall time (workload-specific). |
| F2 | Partially confirmed: width estimation and rendering can repeat field work; the cost depends on selected fields and layout. |
| F3 | Confirmed structurally and optimized; targeted AArch64 link-resolution modes measured 7.12–9.18% lower mean wall time. Other metadata calls remain path-dependent. |
| F4 | Confirmed structurally: `Inodemap` receives every non-directory passed to `UpdateEstimations`, without an `st_nlink > 1` check. |
| F5 | Character-at-a-time output path confirmed; syscall savings from `setvbuf` are not established. |
| F6 | The stated locale-based explanation is not supported by the current formatter; numeric stream formatting remains a separate optimization hypothesis. |
| F7 | Confirmed for the legacy `Gprintf` path: a `PrintfProxy` parses the format on each invocation. `Gformat` bypasses it when available. |
| F8 | The supplied date-cost estimates are overstated or describe the wrong branches. |
| F9 | Partially confirmed: cache hits still lock and copy the cached string; NSS locking is limited to misses. |
| F10 | Partially confirmed: full paths and path-based metadata lookups are used; the claimed temporary allocation does not apply to `std::string` callers. |
| F11 | Mutexes are present in the normal GCC build; this confirms a possible cost, not that the lock is a measured hotspot. |

For findings without a cited benchmark, “confirmed” means the code path exists; source inspection alone does **not** establish its runtime impact. The F1/F3 measurements below are specific to one AArch64 build and fixture and should not be generalized. The supplied inventory enumerates F1–F12 (12 findings) while also citing 11 translation units; this review treats the F-numbers as finding IDs, not as a translation-unit count.

## Finding details

### F1 — repeated width estimation

At the reviewed baseline, `PrintAllFilesCollectedSoFar()` used a binary search to choose the vertical layout, and each `acceptable()` attempt called `UpdateEstimations()` for the items it visited. The accepted layout was evaluated once more before rendering. This draft now computes one `Estimation` per sorted item before the search and merges those cached widths into each column during each attempt, avoiding repeated calls to `GetName()`, `GetSize()`, owner/group lookup, and `Inodemap.insert()` from the search loop. The `TellMe()` rendering path and width rules are unchanged. See the F1 implementation in `main.cc:683–694` and `main.cc:752–772`; the surrounding search is in `main.cc:747–803`, with rendering in `main.cc:371–451`.

### F2 — estimation work repeated by rendering

`UpdateEstimations()` computes name/size widths and, when requested, owner/group names. `TellMe()` computes the corresponding output values again. This supports a field- and mode-dependent duplication claim, but not a blanket claim that a separate directory scan performs all the same work: with prescan enabled, `SingleFile()` first stores the name and metadata; estimation happens later when the collected entries are laid out. The F1 cache now removes repeated estimation across vertical-layout trials, but estimation versus final rendering still occurs separately. See `main.cc:696–733`, `main.cc:752–803`, `main.cc:875–890`, and `main.cc:325–451`.

### F3 — symlink resolution

At the reviewed baseline, `GetName()` called `LinkTarget()` before and after target inspection when `Links >= 2`, producing two `readlink()` calls plus `stat()` and a conditional `lstat()`. This draft passes an optional output string to `LinkTarget()`, allowing the fixed target path used for metadata and the raw target used for rendering to come from the same readlink buffer. The branch therefore saves one `readlink()`; the `stat()`/`lstat()` behavior remains conditional, and other paths (such as size formatting) are unchanged. The result is reused only within one `GetName()` call. See the current implementation at `getname.cc:92–125`, `strfun.cc:38–59`, and `stat.h:30–65`.

### F4 — inode-map insertions

`UpdateEstimations()` inserts every non-directory into `Inodemap`; `Inodemap::insert()` checks only whether the map is enabled and whether the key is already present. A blanket `st_nlink > 1` guard is not applied: with `-l0`, several symlink paths to the same single-link target currently share the target inode and can produce hardlink annotations, so skipping those entries would change existing output. The F1 cache does avoid repeating these insert attempts across vertical-layout trials. See `main.cc:225–266` and `main.cc:696–704`.

### F5 — character-at-a-time output

`Gputch()` routes ordinary output characters through `putchar()` (or the platform-specific `putch` path), so the per-character function-call path exists. `stdout` is a buffered C stream, however; the source alone does not imply one `write(2)` syscall per character or show that `setvbuf()` would reduce syscall counts. A batching change should be evaluated for CPU/locking cost and must preserve output, color handling, paging, and column tracking. See `cons.cc:272–291` and `cons.hh:25–42`.

### F6 — numeric formatting and locale

The examined `printf.cc` path contains no `imbue(std::locale(""))` or other explicit locale setup. Arithmetic values formatted as strings do use a stream with formatting manipulators, so replacing that mechanism could still be evaluated independently. The reported locale overhead is not confirmed, and any `to_chars` change would need to preserve existing formatting semantics. See `printf.cc:369–406`.

### F7 — format parsing

The legacy `Gprintf()` path constructs a `GprintfProxy`, which owns a `PrintfProxy`; the `PrintfProxy(std::string_view)` constructor calls `MakeFrom()` on that format. Therefore the format is parsed on each call through this path. When `HAVE_FORMAT` is enabled, `Gformat()` instead uses `std::format` and bypasses `PrintfProxy`. This is a plausible repeated cost for frequently invoked legacy formats, but its runtime impact has not been measured. See `cons.hh:45–68`, `cons.hh:91–108`, and `printf.hh:201–204`.

### F8 — date formatting

The per-entry rendering branches do not match the larger call counts stated in the supplied report:

- `%u`: one `time()` and one `ctime()` call.
- `%z`: one `time()` and two `localtime()` calls.
- Other date formats: one `localtime()` call passed to `strftime()`.

The width estimator handles `%u` and `%z` as fixed widths; for other formats it calls `strftime()` with a zero-initialized `tm`, not the file's timestamp. These are code-path counts, not a profile. See `main.cc:400–451` and `main.cc:610–623`.

### F9 — passwd/group lookup caches

`cached_lookup()` takes its cache mutex on both hits and misses and returns the cached `std::string` by value. The NSS mutex is taken inside the lookup callback, so it is used on a cache miss rather than every hit. The optional UID/GID preload is disabled by default in `CMakeLists.txt`. These observations support a possible lock/copy cost but do not establish its share of total runtime. See `pwfun.cc:19–35`, `pwfun.cc:69–103`, and `CMakeLists.txt:24–31`.

### F10 — path construction and metadata lookup

`ScanDir()` builds a full path string for each directory entry before calling `SingleFile()`, and the stat wrappers ultimately call path-based `stat()`/`lstat()`. However, `try_stat(const std::string&)` and `try_lstat(const std::string&)` pass `c_str()` directly; the extra owned string is specific to the `std::string_view` overload. Replacing path-based lookup with `fstatat()` would require a directory-handle-aware design and compatibility checks, not just changing the wrapper. See `main.cc:995–1013` and `stat.h:30–65`.

### F11 — DFA mutex

`DFA_DISABLE_MUTEX` is defined under `__SUNPRO_CC`; absent an external build definition, a normal GCC build therefore includes the matcher mutex (or shared-timed mutex). This confirms that synchronization is present, but the source review did not establish that it is a hot path or that removing it would be safe for all uses. Any change should remain narrowly scoped and preserve existing lock semantics. See `dfa_match.hh:6–16`, `dfa_match.hh:140–152`, and the lock macros in `dfa_match.cc:497–505`.

## Implemented in this draft

### F1 — reuse per-item width estimates

The vertical layout now builds an `Estimation` vector once in directory order and merges those per-item widths into the candidate column maxima during the binary search. This retains the existing `Inodemap` insertion order and leaves the rendering logic unchanged.

### F3 — reuse symlink target read

`GetName()` now obtains the fixed path used for target metadata and the raw link target used for display from one `readlink()` call. It preserves the existing conditional `stat()`/`lstat()` probes and only reuses data within that `GetName()` invocation.

### Cache the date width used by layout estimation

When a date field is active, its estimate is now computed once after command-line options and fields are parsed. `CalculateRowWidth()` reuses that value instead of calling `strftime()` during every candidate-row calculation in vertical-layout search. The existing `%u` and `%z` widths are unchanged; other formats still estimate with a zero-initialized `tm`, and rendered dates are untouched.

No CLI option, default, output format, sorting, palette, or paging behavior was intentionally changed.

## Validation and limitations

For the F1 and F3 candidates, earlier local Release and Debug ASan/UBSan builds each passed all 15 CTest tests. The date-width candidate was separately built in Release (static + LTO) and Debug ASan/UBSan; both CTest runs also passed 15/15. These supplementary builds used GCC 12.2.0 (CMake 3.31.6 for the date-width candidate), below the requested GCC 14 floor, so they do not replace the GCC 14+ CI matrix.

Byte comparisons were identical for aliases `-a0` through `-a6` on a fixture containing hidden files, a directory, a hardlink, and valid, absolute, dangling, directory, and chained symlinks. Explicit `-l0` through `-l5` were also compared after the aliases (42 combinations); a separate vertical-layout comparison with link modes 2, 3, and 5 was byte-identical. The date-width candidate also matched the baseline byte-for-byte for `-a0` through `-a6` and three custom `-F` date formats, including vertical layout.

A local 6,000-entry vertical-layout microbenchmark (3 warm-ups, 15 alternating timed runs per version, same host) measured median wall time of 43.4 ms for baseline and 29.0 ms for the F1 candidate (33.3% lower). This is indicative only: the samples include scheduler noise, and this fixture/timing method is not a substitute for the CI profiler. A separate 10,000-symlink probe observed 139.0 ms vs. 127.4 ms for `-l3`, but the `-l0` control also varied; no attributable F3 wall-time gain is claimed from that run.

### Additional AArch64 main-versus-draft benchmark

User-provided Hyperfine 2.0.0 JSON results compare Linux/AArch64 Release binaries built with static linking and LTO, with 50 samples per version on the same symlink fixture. The build configuration was:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDIRR_ENABLE_LTO=ON -DDIRR_STATIC=ON -DDIRR_ENABLE_SANITIZER=OFF
cmake --build build --parallel 8
```

The commands used the same `-A -c -X80` display options and varied the mode (`-a0` through `-a6`, `-C`, `-e`, or `-w`). The table reports mean wall-clock time; positive percentages mean the draft was faster. The approximate 95% Welch intervals use the reported means and standard deviations, treating each version's 50 samples as independent (not paired or bootstrap intervals).

| Mode | Main → draft (ms) | Draft faster | Approx. 95% interval |
| --- | ---: | ---: | ---: |
| `a0` | 99.58 → 99.32 | 0.27% | −2.18…+2.72% |
| `a1` | 226.21 → 209.81 | 7.25% | +6.59…+7.91% |
| `a2` | 100.27 → 99.21 | 1.06% | −0.52…+2.65% |
| `a3` | 97.94 → 95.79 | 2.20% | −0.54…+4.94% |
| `a4` | 103.62 → 100.89 | 2.63% | +0.71…+4.56% |
| `a5` | 226.00 → 208.19 | 7.88% | +6.64…+9.13% |
| `a6` | 220.09 → 201.69 | 8.36% | +7.33…+9.40% |
| `-C` | 211.20 → 191.82 | 9.18% | +7.67…+10.68% |
| `-e` | 181.17 → 168.28 | 7.12% | +5.23…+9.01% |
| `-w` | 164.98 → 152.67 | 7.46% | +6.49…+8.43% |

The `Links >= 2` modes (`a1`, `a5`, `a6`, `-C`, and `-e`) improved by 7.12–9.18%; their mean system time fell by 9.7–13.3%, consistent with the F3 `readlink()` reuse. In the current source, `opt_a(1)` leaves `Links` at its default value of 3, despite the help text listing `-l1`, so `a1` also exercises F3. The `-w` alias sets `Links=1` and enables vertical layout, isolating the F1 path among these changes: wall time fell 7.46%, CPU cycles fell 11.11%, and instructions fell 12.27%. Mean peak resident memory rose from 4.61 to 4.98 MiB (+7.95%), an observed memory tradeoff of caching per-entry estimates.

The `a0`, `a2`, and `a3` intervals include zero. `a4` has a small positive result but does not exercise either optimized path; it is not attributed to F1 or F3. These are targeted results on one host and fixture, not a general performance guarantee.

A supplementary date-width benchmark used a 50,000-file vertical listing with a date field, Release/static/LTO binaries, and 30 randomized subprocess runs per version on this x86_64 host. Baseline mean wall time was 112.68 ms and the candidate mean was 110.65 ms (1.80% lower); a rough independent-sample Welch 95% interval for the improvement was −1.44…+5.05%, so the timing is inconclusive. The source-level reduction in repeated `strftime()` calls is certain, but no end-to-end speedup is claimed from this noisy run.

`scripts/profile-hot-path.sh` ran locally with baseline/candidate binaries, but Hyperfine, perf, strace, and Heaptrack were unavailable, so the script recorded `NOT RUN` for those probes. The configured PR profiling workflow builds base and head identically, runs the same script with its profiling tools, and uploads a report artifact for each push. CMake was installed outside the repository for local validation; no project dependency or workflow was added or changed.
