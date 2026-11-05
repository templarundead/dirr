# Performance audit findings (F1–F11)

**Reviewed revision:** `0495bf12094b059ad3cb08fd100ebdc065d411d5`

**Review type:** source inspection plus targeted local benchmarks. The AArch64 comparison below is specific to the tested host, build, and fixture; it is not a generalized performance guarantee.

**Scope:** findings F1–F11 from the supplied audit. F12/PGO is intentionally out of scope at the user's request.

## Summary

| Finding | Assessment |
| --- | --- |
| F1 | Confirmed structurally and optimized in this draft. The earlier targeted AArch64 `-w` comparison measured 7.46% lower mean wall time; a later full-branch AArch64 batch measured 55.18% lower on `-w`, but combines F1, color sorting, and other draft changes. |
| F2 | Confirmed structurally and partly optimized. Repeated symlink resolution and per-call name copies are removed, and passwd/group repeats now stay out of the shared map; the duplicated size-column formatting was measured at ≤0.5% and deliberately left unchanged (see the F2 follow-up section). |
| F3 | Confirmed structurally and optimized: readlink reuse measured 7.12–9.18% lower mean wall time in the earlier AArch64 comparison; a successful-target-stat cache reduced a local 50k-link workload by 9.62% mean wall time and halved target `stat()` calls. Both results are fixture-specific. |
| F4 | Confirmed structurally: `Inodemap` receives every non-directory passed to `UpdateEstimations`, without an `st_nlink > 1` check. |
| F5 | Confirmed and optimized in this draft: the per-character `putchar()` call is replaced by the inlined, lock-free `putc_unlocked()`, measured at 3.5–8.1% lower CPU time. An own 64 KiB output buffer was also implemented and measured: it packs the stdio calls but does not lower CPU time (`+0.6…+1.1%`), so it was rejected. `setvbuf()` is not needed; syscall counts were never the cost. |
| F6 | The stated locale-based explanation is not supported by the current formatter. A `to_chars` trial for link counts showed no measurable speedup on the corrected local benchmark. |
| F7 | Confirmed for the legacy `Gprintf` path: a `PrintfProxy` parses the format on each invocation. `Gformat` bypasses it when available. |
| F8 | The supplied date-cost estimates are overstated or describe the wrong branches; a corrected local date-width benchmark measured 31.54% lower mean wall time on its targeted workload. |
| F9 | Partially confirmed and partly optimized: cache hits still copy the cached string, but this draft now answers repeated ids from a thread-local one-entry cache without taking the shared lock; NSS locking is limited to misses. |
| F10 | Partially confirmed: full paths and path-based metadata lookups are used; the claimed temporary allocation does not apply to `std::string` callers. |
| F11 | Mutexes are present in the normal GCC build; this confirms a possible cost, not that the lock is a measured hotspot. |
| Additional color-sort path | `c/C` calls the DFA twice per comparator invocation; a per-entry key cache improved local 50k-file `--sort=c`/`--sort=pcm` timings by 24.77%/29.22% mean. |

For findings without a cited benchmark, “confirmed” means the code path exists; source inspection alone does **not** establish its runtime impact. The historical AArch64 F1/F3 results and the later local microbenchmarks are specific to their host, build, and fixture and should not be generalized. The supplied inventory enumerates F1–F12 (12 findings) while also citing 11 translation units; this review treats the F-numbers as finding IDs, not as a translation-unit count.

## Finding details

### F1 — repeated width estimation

At the reviewed baseline, `PrintAllFilesCollectedSoFar()` used a binary search to choose the vertical layout, and each `acceptable()` attempt called `UpdateEstimations()` for the items it visited. The accepted layout was evaluated once more before rendering. This draft now computes one `Estimation` per sorted item before the search and merges those cached widths into each column during each attempt, avoiding repeated calls to `GetName()`, `GetSize()`, owner/group lookup, and `Inodemap.insert()` from the search loop. The `TellMe()` rendering path and width rules are unchanged. See the F1 implementation in `main.cc:683–694` and `main.cc:752–772`; the surrounding search is in `main.cc:747–803`, with rendering in `main.cc:371–451`.

### F2 — estimation work repeated by rendering

`UpdateEstimations()` computes name/size widths and, when requested, owner/group names. `TellMe()` computes the corresponding output values again. This supports a field- and mode-dependent duplication claim, but not a blanket claim that a separate directory scan performs all the same work: with prescan enabled, `SingleFile()` first stores the name and metadata; estimation happens later when the collected entries are laid out. The F1 cache now removes repeated estimation across vertical-layout trials, but estimation versus final rendering still occurs separately. See `main.cc:696–733`, `main.cc:752–803`, `main.cc:875–890`, and `main.cc:325–451`.

### F3 — symlink resolution

At the reviewed baseline, `GetName()` called `LinkTarget()` before and after target inspection when `Links >= 2`, producing two `readlink()` calls plus `stat()` and a conditional `lstat()`. This draft passes an optional output string to `LinkTarget()`, allowing the fixed target path used for metadata and the raw target used for rendering to come from the same readlink buffer, saving one `readlink()` per `GetName()` call.

The draft also gives each listed symlink a lazy `LinkTargetStatCache` when the name field is estimated with `Links >= 2`. A successful target `stat()` mode is reused when the same raw link text is read during final rendering; failed `stat()` results are not cached, and a changed link text invalidates the cached mode. Conditional `lstat()` behavior and other metadata paths (such as size formatting) are unchanged. Because a successful mode may now be reused between width estimation and rendering, a target replaced in place during that interval could retain the earlier mode for type-suffix and color decisions; directory listings are otherwise unchanged. See `getname.cc:92–150`, `getname.hh:12–21`, `main.cc:714–737`, `strfun.cc:38–59`, and `stat.h:30–65`.

### F4 — inode-map insertions

`UpdateEstimations()` inserts every non-directory into `Inodemap`; `Inodemap::insert()` checks only whether the map is enabled and whether the key is already present. A blanket `st_nlink > 1` guard is not applied: with `-l0`, several symlink paths to the same single-link target currently share the target inode and can produce hardlink annotations, so skipping those entries would change existing output. The F1 cache does avoid repeating these insert attempts across vertical-layout trials. See `main.cc:225–266` and `main.cc:696–704`.

### F5 — character-at-a-time output

`Gputch()` routes ordinary output characters through `putchar()` (or the platform-specific `putch` path), so the per-character function-call path exists. `stdout` is a buffered C stream, however; the source alone does not imply one `write(2)` syscall per character or show that `setvbuf()` would reduce syscall counts. A batching change should be evaluated for CPU/locking cost and must preserve output, color handling, paging, and column tracking. See `cons.cc:272–291` and `cons.hh:25–42`.

The call counts are now measured rather than inferred: a 20,000-file listing issues 0.77–1.38 million `putc()` calls and 120,000-160,000 `fwrite()` calls per run, and no `write(2)` calls reach the descriptor before the stream is flushed. `setvbuf()` would therefore address the wrong layer — the cost is the locked per-character call itself. See the F5 series at the end of this document for the measurements, the adopted change, and the buffer experiment that was rejected on measurement.

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

### Additional observation — color sort

`StatItem::operator<` evaluates both `GetNameAttr()` calls for every comparison that reaches a `c` or `C` key (`main.cc:556–557`). Each call reaches `DFA_Matcher::Test()` through `GetNameAttr()` → `NameColor()`; `Test()` takes the shared lock once and scans the basename until a match or the terminating byte (`colouring.cc:16–17`, `setfun.cc:579–582`, `dfa_match.cc:678–699`). Thus `--sort=c`/`--sort=C` can perform about two locked DFA tests per comparison, or roughly 266k calls for a 10k-element `N log₂ N` estimate. The exact count depends on `std::sort`, and a successful DFA match may terminate before the full name is scanned.

Color sorting is absent from the default `pmgU` order, but the built-in `-w`/`-W` presets select `pcm`/`PCM`; their `c/C` key is reached for items tied on the preceding class key. Source inspection cannot establish how often users choose these options.

## Implemented in this draft

### F1 — reuse per-item width estimates

The vertical layout now builds an `Estimation` vector once in directory order and merges those per-item widths into the candidate column maxima during the binary search. This retains the existing `Inodemap` insertion order and leaves the rendering logic unchanged.

### F3 — reuse symlink target data

`GetName()` obtains the fixed path used for target metadata and the raw link target used for display from one `readlink()` call. A lazy per-entry cache also reuses a successful target mode across name-width estimation and rendering when the raw link text is unchanged. Failed target `stat()` calls are still retried, and `lstat()` behavior is unchanged. The cache is only enabled for listed symlinks when `Links >= 2` and the name field is active.

### Cache the date width used by layout estimation

When a date field is active, its estimate is now computed once after command-line options and fields are parsed. `CalculateRowWidth()` reuses that value instead of calling `strftime()` during every candidate-row calculation in vertical-layout search. The existing `%u` and `%z` widths are unchanged; other formats still estimate with a zero-initialized `tm`, and rendered dates are untouched.

### Format link counts with `to_chars`

When `HAVE_CHARCONV` is available, link-count width estimation and output use `std::to_chars()` into a stack buffer and pass the resulting view to `Gwrite()`. Builds without charconv retain the `std::to_string()` fallback. Decimal text and the existing field-padding path are unchanged.

### Cache the color sort key

When the active sort string contains `c` or `C`, the pre-sort pass computes each `StatItem`'s `GetNameAttr()` once. The comparator uses the cached integer, and `TellMe()` reuses it for the initial filename attribute. Other sort orders leave the optional cache empty and keep the existing rendering path. The compared value is still the exact per-item result of `GetNameAttr(stat, NameOnly(name))`; the sort remains `std::sort`.

### F5 — unlock the per-character output path

dirr writes from a single thread, so the `FILE` locking that `putchar()` and `fwrite()` perform buys nothing. `Gputch()` now emits characters with `putc_unlocked()`, and the padding-run and attribute-escape writes use `fwrite_unlocked()`; both are available under `-std=c++23` with glibc and expand to inline buffer stores, so the per-character call does not even reach the PLT in the Release build. `FlushSetAttr()` already returns immediately when the attributes match, so `Gputch()` now tests that condition itself instead of calling through on every character. The DJGPP console path (`putch`/`putchar`) is unchanged.

No CLI option, default, output format, sorting order, palette, or paging behavior was intentionally changed.

## Validation and limitations

The current combined candidate, including the successful target-stat cache, date-width cache, link-count `to_chars`, and color-sort key cache, passed all 15 CTest tests in both Release (static + LTO) and Debug ASan/UBSan builds. These supplementary builds used GCC 12.2.0 and CMake 3.31.6, below the requested GCC 14 floor, so they do not replace the GCC 14+ CI matrix.

The current candidate and its immediate parent produced byte-identical output for aliases `-a0` through `-a6` on a fixture containing hidden files, a directory, a hardlink, and valid, absolute, dangling, directory, and chained symlinks. Explicit `-l0` through `-l5` with attached filename field `-f.f` also matched on a 10,000-symlink fixture. For the color-sort change, outputs matched for `-o c`, `-o C`, `-o pcm`, `-o PCM`, `--sort=n`, and `-w` on a 50,000-file fixture. The date-width path matched for five formats using attached `-f.f_.d` and `-F<format>` arguments, and the link-count path matched for `-f.h_.f` on a 50,000-file fixture. These corrected invocations attach each specification to its option; a separate `-f` argument would be treated as an operand rather than a field value.

A local 6,000-entry vertical-layout microbenchmark (3 warm-ups, 15 alternating timed runs per version, same host) measured median wall time of 43.4 ms for baseline and 29.0 ms for the F1 candidate (33.3% lower). This is indicative only: the samples include scheduler noise, and this fixture/timing method is not a substitute for the CI profiler. An earlier 10,000-symlink probe was inconclusive because its `-l0` control also varied; it is superseded by the controlled F3 comparison below.

### Earlier AArch64 main-versus-draft benchmark (F1/F3 readlink reuse)

User-provided Hyperfine 2.0.0 JSON results compare Linux/AArch64 Release binaries built with static linking and LTO, with 50 samples per version on the same symlink fixture. This comparison predates the later date-width, `to_chars`, successful target-stat-cache, and color-sort-key additions; it does not measure those changes. The build configuration was:

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

### Follow-up AArch64 per-option benchmark (2026-11-05)

A later, separate user-provided Hyperfine 2.0.0 batch compared `main` and `draft` on Linux/AArch64 Release binaries built with static linking and LTO, with 50 samples per version and the same per-option workload for each pair. The build configuration was `Release`, `DIRR_ENABLE_LTO=ON`, `DIRR_STATIC=ON`, and `DIRR_ENABLE_SANITIZER=OFF`. These files are a distinct series from the 10,000-symlink profiling artifacts dated 2026-11-04; the two series are not combined below.

The table reports mean wall time. Positive values mean the draft was faster. Approximate 95% intervals were calculated from the reported means and standard deviations assuming independent samples; they are not paired or bootstrap intervals.

| Mode | Main → draft (ms) | Draft faster | Approx. 95% interval |
| --- | ---: | ---: | ---: |
| `-a0` | 278.57 → 278.72 | −0.05% | −0.84…+0.73% |
| `-a1` | 299.46 → 303.60 | −1.38% | −2.06…−0.71% |
| `-a2` | 264.60 → 264.91 | −0.12% | −0.76…+0.52% |
| `-a3` | 253.96 → 253.27 | +0.27% | −0.72…+1.27% |
| `-a4` | 291.30 → 289.58 | +0.59% | −0.15…+1.34% |
| `-a5` | 336.27 → 338.61 | −0.69% | −1.19…−0.20% |
| `-a6` | 311.34 → 312.83 | −0.48% | −1.06…+0.10% |
| `-C` | 293.41 → 286.75 | +2.27% | +1.33…+3.21% |
| `-e` | 240.95 → 236.82 | +1.71% | +0.57…+2.86% |
| `-w` | 515.15 → 230.89 | +55.18% | +54.58…+55.78% |

In this batch, `-c` disables colored output; it does not request color sorting. `-C` selects multicolumn output but does not itself enable vertical layout, while `-e` disables sorting. Only `-w` both enables vertical layout and uses `Sorting=pcm`, so it directly exercises the F1 estimate cache and the `c` sort key. Its CPU cycles fell 69.73% and instructions 68.74%; peak RSS rose from 14.26 to 17.85 MiB (+25.2%). Peak RSS was also higher in the other modes (about 7–11%, depending on mode).

The `-w` result is a large net improvement for the complete main-versus-draft comparison, not an isolated measurement of F1 or the color-key cache. The `-a1` and `-a5` slowdowns are small (−1.38% and −0.69%) and do not exercise either the vertical-layout search or the `c` sort key. The other `-a` results are close to parity; `-C` and `-e` show modest gains from the complete branch comparison. All results remain specific to this host, build, and workload.

### Color-sort key benchmark

The pre-cache baseline and candidate were dynamic Release/LTO binaries built with GCC 12.2.0 and CMake 3.31.6 on this x86_64 host. A 50,000-regular-file fixture was tested with output redirected to `/dev/null`, 3 warm-ups and 30 randomized subprocess runs per version for each sort order (`LC_ALL=C`, `TZ=UTC`). Mean/median wall times and the rough independent-sample Welch 95% interval for mean improvement were:

| Sort order | Baseline mean/median (ms) | Candidate mean/median (ms) | Mean reduction | Approx. 95% interval |
| --- | ---: | ---: | ---: | ---: |
| `--sort=c` | 213.842 / 211.339 | 160.869 / 157.884 | 24.77% | 21.81…27.73% |
| `--sort=pcm` | 241.429 / 235.316 | 170.879 / 164.530 | 29.22% | 25.77…32.67% |
| `--sort=n` control | 164.456 / 162.847 | 162.723 / 162.702 | 1.05% | −1.87…+3.98% |

The `--sort=n` control shows no measurable regression. These timings are local and workload-specific; the per-entry `std::optional<int>` adds storage to each `StatItem`, and peak memory was not measured.

### Corrected date-width benchmark

The corrected 50,000-file vertical-layout benchmark used `-C -vc -A -c -X80 -f.f_.d`, with the field specification attached to `-f`. Release/static/LTO binaries were compared in 30 randomized subprocess runs per version on this x86_64 host. Baseline mean/median wall time was 222.287/214.589 ms; the date-width candidate measured 152.180/151.945 ms, a 31.54% lower mean. The rough independent-sample Welch 95% interval for the improvement was 28.79–34.28%. The earlier 112.68/110.65 ms figures used an invalid, separated `-f` argument and are withdrawn.

### Corrected link-count `to_chars` benchmark

The corrected 50,000-file benchmark used the attached field specification `-f.h_.f`, 30 randomized runs per version, and Release/static/LTO binaries. Baseline mean/median wall time was 146.779/142.400 ms; the `to_chars` candidate measured 146.775/146.726 ms. The mean difference rounds to 0.00%, and the rough independent-sample Welch 95% interval for improvement was −5.15…+5.15%; no speedup was detected. The earlier 105.11/106.34 ms figures passed `.h_.f` as a separate operand after `-f`, so they are invalid and withdrawn. The candidate remains a low-risk trial pending measurements on the target AArch64 host.

### Corrected F3 target-stat benchmark and call-count probe

A 50,000-symlink fixture pointed every link at the same target. Baseline and target-cache candidate were dynamic Release/LTO binaries differing only by the target-stat cache; 30 randomized subprocess runs per version used `-l3 -A -c -X80 -f.f`. Mean/median wall time fell from 378.918/370.942 ms to 342.456/335.488 ms (9.62% lower mean); the rough independent-sample Welch 95% interval for improvement was 6.51–12.73%. A custom `LD_PRELOAD` counter filtered calls to the resolved `targets/file` path and recorded 100,000 target `stat()` calls for the baseline versus 50,000 for the candidate (two per link reduced to one per link); the single-link probe likewise changed from two calls to one. This confirms the repeated target-stat path and the intended call-count reduction, while the wall-time result remains specific to this synthetic fixture and host. Heaptrack was unavailable, so the memory overhead of the lazily allocated per-symlink cache was not measured.

`scripts/profile-hot-path.sh` ran locally against the target-stat and color-sort baseline/candidate pairs, but its built-in `-l0`/`-l3` workload does not exercise `c/C`; Hyperfine, perf, strace, and Heaptrack were also unavailable, so those probes were recorded as `NOT RUN`. The targeted randomized subprocess timings above are supplementary, not substitutes for the configured CI profiler. The PR workflow is configured to upload a report on each push; the prior artifact download failed with `EOF`, so this audit contains no CI profile for the current color-sort change. CMake was installed outside the repository for local validation; no project dependency or workflow was added or changed.

### F2 follow-up: estimation-versus-rendering duplicates (2026-11-05)

This series isolates the F2 duplicates on x86_64 (Debian 12, GCC 12.2.0, CMake 4.4.4, Release/LTO binaries built by the project's own CMake lists). Two independent instruments were used, because wall-clock differences of a few percent are not resolvable on a shared two-vCPU host:

- `rdtsc` counters compiled into the estimating and the rendering passes, reporting cumulative cycles and call counts per instrumented call site. The build is otherwise identical to the Release build; each instrumented region carries roughly 25 cycles of measurement overhead, so the totals below are slight overestimates.
- `LD_PRELOAD` counters for `readlink`/`stat`/`lstat`/`malloc`, plus `maxrss` taken from `wait4()` for each child process.

Fixtures were flat directories of 20,000 entries: regular files with short names, regular files with 60-character names, and symlinks, plus a mixed directory with valid, absolute, relative, dangling, directory and chained links, a FIFO, a socket and hardlinks.

#### What the duplication costs

Default long listing (`-A -c`) of 20,000 regular files, TSC cycles per run and per phase:

| Instrumented site | Estimating pass | Rendering pass |
| --- | ---: | ---: |
| Size column formatting (`GetSize`) | 1.69M / 20,002 calls | 2.01M / 20,002 calls |
| Passwd/group lookup (`Getpwuid`/`Getgrgid`) | 2.32M / 40,004 calls | 2.46M / 40,004 calls |
| Name column (`GetName`) | 4.61M / 20,002 calls | 11.17M / 20,002 calls |

Against a whole run of roughly 50–66 ms, the repeated size formatting is about 1.1 ms (2.2%) across both passes and the repeated passwd/group lookups about 1.45 ms (2.9%); a fix can only recover the half that the second pass repeats.

#### Changes made

| Commit | Change | Structural effect | Measured effect |
| --- | --- | --- | --- |
| `a4013af` | `LinkTargetStatCache` now holds the link text, the fixed target path and the `stat()`/`lstat()` results; `GetName()` and `GetSize()` share it | `-l3`, 20k symlinks: `readlink` 80,000 → 20,000, `stat` 60,000 → 20,000, `lstat` 40,002 unchanged | `-l3` 173.33 → 111.12 ms (−34.6%), `-l3 -C` 155.53 → 91.15 ms (−39.6%) |
| `776bf35` | `GetName()` takes a `std::string_view` and prints from it | 20k long names: `malloc` 180,356 → 100,356 (default) and 100,292 → 20,292 (`-w -C`) | 2.0–4.0% lower wall time across all layouts |
| `2954ae2` | Thread-local one-entry front cache in `cached_lookup()` | Passwd/group cycles, both passes: 4.79M → 3.45M | ≈0.8% of the run; RSS unchanged (20,232 → 20,208 KiB on the symlink fixture) |
| `bb9cc29` | Cache keeps a 24-byte `StatSummary` plus the `lstat` mode instead of two `struct stat` and a copy of the item path | Same syscall counts as `a4013af` | Peak RSS for `-l3`, 20k symlinks: 20.2 → 13.3 MB (11.7 MB without the cache) |

Wall-clock series for the complete branch (25 interleaved runs per configuration, median of per-run ratios, approximate 95% interval from the sample standard deviation):

| Workload | Baseline → draft (ms) | Change |
| --- | ---: | ---: |
| 20k symlinks, `-l3` | 173.33 → 111.12 | −34.6% (−35.7…−33.5) |
| 20k symlinks, `-l3 -C` | 155.53 → 91.15 | −39.6% (−41.3…−37.9) |
| 20k symlinks, `-l0` | 50.56 → 49.26 | −2.8% (−4.3…−1.3) |
| 20k symlinks, `-l3 -w` | 31.41 → 30.18 | −3.9% (−4.7…−3.1) |
| 20k files, default | 50.27 → 49.33 | −2.7% (−5.0…−0.3) |
| 20k files, `-w` | 34.44 → 33.55 | −3.1% (−5.7…−0.6) |
| 20k files, `-w -C` | 37.08 → 36.51 | −2.2% (−3.9…−0.6) |
| 20k long names, default | 67.32 → 64.90 | −2.0% (−2.8…−1.3) |
| 20k long names, `-w` | 57.38 → 55.38 | −2.8% (−4.1…−1.6) |
| 20k long names, `-w -C` | 53.83 → 52.89 | −1.5% (−2.4…−0.5) |

#### Experiments rejected on measurement

- **Formatting the size column once instead of twice.** A per-item `SizeTextCache` (formatted text plus the colour decision, keyed by separator variant and the item's `st_mode`/`st_size`/`st_rdev`) was implemented and verified byte-identical over 282 general comparisons and 160 additional `--format` comparisons covering `.s`, `.z`, `.S` and thousands separators with `-w`, `-C` and `-e`. It cannot recover more than the rendering pass's 2.01M cycles, and after paying for the extra string copies the net gain is ≤0.5%, while peak RSS rose by 0.75–3.5 MB (20k files 10,688 → 14,148 KiB; 20k symlinks 20,228 → 23,168 KiB; long names 12,780 → 14,788 KiB). Reverted, not committed.
- **Carrying the resolved owner/group names per item.** Removing the rendering pass's repeat lookups (`pwgr` 2.45M → 0 cycles, ≈1.5%) costs 1.0–1.5 MB of peak RSS because `StatItem` grows inside a doubling vector (20k files 10,688 → 11,700 KiB; 20k symlinks 20,228 → 21,748 KiB); storing interned pointers instead of strings still cost about 1 MB on the regular-file fixture. The thread-local front cache was kept instead: it recovers about half the gain at no measurable memory cost.

Verification for this series: Release and Debug/ASan+UBSan CTest pass 15/15, sanitizer runs over the four fixtures are clean, and 282 byte-for-byte comparisons of baseline versus draft output (`-a0`…`-a6` with `-A -c`, `-w`, `-C`, `-w -C`; `-l0`…`-l5`; `-e`; default and custom `--format` field orders on four directories) produced no differences. Hyperfine, perf, strace and Heaptrack are still unavailable on this host, so `scripts/profile-hot-path.sh` again recorded `NOT RUN` for them.

The cycle counts come from instrumented builds and the wall-clock figures from separate Release binaries, so the two are not combined into a single percentage; all numbers describe this host, compiler, build and fixture.

### F5 output-path series (2026-11-05)

This series isolates the character-at-a-time output path on the same x86_64 host (Debian 12, GCC 12.2.0, CMake 4.4.4, Release/LTO binaries from the project's own CMake lists). Two independent instruments were used, for the same reason as in the F2 series: wall-clock differences of a few percent are not resolvable on a shared two-vCPU host.

- An `LD_PRELOAD` counter over `putc`/`fputc`, `putc_unlocked`, `fwrite`/`fwrite_unlocked`, `printf` and `write(1)`, reporting call and byte counts per run. GCC compiles `putchar()` into a `putc()`/`_IO_putc` call, and the Release binary contains no `putchar` symbol at all, so counting `putchar` alone would have measured nothing.
- Wall time and child CPU time (`ru_utime + ru_stime` from `wait4`) of alternating A/B subprocess pairs, five listings per sample, reporting the minimum and the median over 20–25 rounds, with a reversed-order repeat to expose order bias.

Fixtures were the same flat 20,000-entry directories as in the F2 series (short names, 60-character names, symlinks), listed with `-A -c -X80` and output to `/dev/null`, `LC_ALL=C`, `TZ=UTC`, `COLS=80`.

#### Call counts before the change

| Fixture | `putc()` calls | `fwrite()` calls (bytes) | `printf` | `write(1)` before flush |
| --- | ---: | ---: | ---: | ---: |
| 20k regular files | 772,185 | 120,029 (228,095) | 0 | 0 |
| 20k long names | 1,380,186 | 120,033 (200,151) | 0 | 0 |
| 20k symlinks, `-l3` | 1,080,190 | 160,031 (240,122) | 0 | 0 |

Two conclusions follow from the counts. First, the per-character path is only 26–32 bytes per listed name here, so its total is 0.8–1.4 million locked calls per run, not the tens of millions a per-column estimate suggests. Second, the attribute writer is not the frequent operation: roughly six `fwrite()` calls per file survive even with `-c`, which is the padding runs shorter than five characters rather than a flush per attribute change. No `write(2)` call reaches the descriptor during the listing, so `setvbuf()` would only change where an existing stdio buffer flushes.

#### Changes made

| Commit | Change | Structural effect |
| --- | --- | --- |
| `b35b4ef` | `Gputch()` emits through `putc_unlocked()`; padding runs and attribute escapes use `fwrite_unlocked()`; `Gputch()` tests `TextAttr != OldAttr` before calling `FlushSetAttr()` | `putc` calls 772,185 → 0 (inlined, no PLT symbol left in the binary); `fwrite` calls stay 120,029 but unlocked |

Wall time and CPU time, candidate minus baseline (negative = faster):

| Workload | Wall min | Wall median | CPU min | CPU median |
| --- | ---: | ---: | ---: | ---: |
| 20k files, default | −3.76% | −5.69% | −5.31% | −6.26% |
| 20k long names, default | −9.31% | −7.25% | −7.85% | −8.13% |
| 20k symlinks, `-l3` | −3.80% | −3.15% | −3.46% | −4.51% |

The reversed-order repeat (candidate as A) reported +3.67%/+4.89%, +7.14%/+8.66% and +5.44%/+5.48%, i.e. the same candidate advantage, so the result is not an artifact of run order. Long names gain most because they carry the most characters per name; the symlink workload gains least because F3's syscall savings already dominate it.

#### Experiment rejected on measurement

A 64 KiB output buffer inside `cons.cc` — a single `Emit`/`EmitChar`/`FlushOut` sink fed by `FlushSetAttr()`, `Gputch()`, the `ESC[nC` escape and the padding runs, flushed when full, on `'\n'` only when `isatty(1)`, before the pager reads a key, and at exit — was implemented and verified byte-identical (282 comparisons, plus tty, pager, `--help`, `| head` and SIGINT runs). It did reduce the write sizes: the 120,029 stdio calls became 16 blocks of about 62 KiB, and a piped run issues about 16 `write(2)` calls instead of about 250. It did not reduce CPU time:

| Workload | CPU min | CPU median |
| --- | ---: | ---: |
| 20k files, default | +0.68% | +1.63% |
| 20k long names, default | +1.06% | +0.35% |
| 20k symlinks, `-l3` | +0.59% | −1.63% |

The reason is the same measurement that motivated the unlocked call: after `b35b4ef`, `putc_unlocked()` is already an inline pointer store into the stdio buffer, so a second buffer adds two branches per byte and removes nothing. The experiment also produced a real ordering hazard rather than a theoretical one: the default build defines neither `ColourPrints` nor `HAVE_PRINT`, so `argh.cc` writes the `--help` option list to `stdout` with a direct `std::fwrite()`, which overtook the still-buffered copyright header and moved it to the end of the output. The hazard is fixable with a flush hook (that fix was written and verified), but a change that is neutral in time and adds a correctness constraint does not belong in the branch, so it was reverted. The commit `b35b4ef` is the whole of this series.

Verification for this series: Release and Debug/ASan+UBSan CTest pass 15/15; sanitizer runs over the four fixtures are clean; the 282 byte-for-byte comparisons of the reviewed baseline against the candidate (`-a0`…`-a6` with `-A -c`, `-w`, `-C`, `-w -C`; `-l0`…`-l5`; `-e`; `-S`; default and `-X40`) show no differences; tty output captured through a pty is identical apart from the live free-space figure; the pager (`-p`, answered with `q`, spaces and carriage returns) is identical including the `--More--` prompt and its erase sequence; `--help` is identical for both a pipe and a tty; `dirr | head -3` exits with the same status (141) in both; and a SIGINT delivered mid-listing leaves the same complete `^C`/sums/free-space tail in both. Hyperfine, perf, strace and Heaptrack remain unavailable on this host, so `scripts/profile-hot-path.sh` again recorded `NOT RUN` for them.
