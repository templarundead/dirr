# Performance audit findings (F1–F11)

**Reviewed revision:** `0495bf12094b059ad3cb08fd100ebdc065d411d5`

**Review type:** source inspection plus a limited local microbenchmark; no generalized performance claim is made.

**Scope:** findings F1–F11 from the supplied audit. F12/PGO is intentionally out of scope at the user's request.

## Summary

| Finding | Assessment |
| --- | --- |
| F1 | Confirmed structurally and optimized in this draft: per-item widths are computed once and reused during the vertical-layout binary search. |
| F2 | Partially confirmed: width estimation and rendering can repeat field work; the cost depends on selected fields and layout. |
| F3 | Partially confirmed: the `Links >= 2` symlink branch can issue multiple `readlink`/`stat`/`lstat` calls; the report's per-entry totals are not universal. |
| F4 | Confirmed structurally: `Inodemap` receives every non-directory passed to `UpdateEstimations`, without an `st_nlink > 1` check. |
| F5 | Character-at-a-time output path confirmed; syscall savings from `setvbuf` are not established. |
| F6 | The stated locale-based explanation is not supported by the current formatter; numeric stream formatting remains a separate optimization hypothesis. |
| F7 | Confirmed for the legacy `Gprintf` path: a `PrintfProxy` parses the format on each invocation. `Gformat` bypasses it when available. |
| F8 | The supplied date-cost estimates are overstated or describe the wrong branches. |
| F9 | Partially confirmed: cache hits still lock and copy the cached string; NSS locking is limited to misses. |
| F10 | Partially confirmed: full paths and path-based metadata lookups are used; the claimed temporary allocation does not apply to `std::string` callers. |
| F11 | Mutexes are present in the normal GCC build; this confirms a possible cost, not that the lock is a measured hotspot. |

“Confirmed” here means the code path exists. It does **not** establish that it materially affects elapsed time, CPU use, syscalls, or allocations on a representative workload. The supplied inventory enumerates F1–F12 (12 findings) while also citing 11 translation units; this review treats the F-numbers as finding IDs, not as a translation-unit count.

## Finding details

### F1 — repeated width estimation

At the reviewed baseline, `PrintAllFilesCollectedSoFar()` used a binary search to choose the vertical layout, and each `acceptable()` attempt called `UpdateEstimations()` for the items it visited. The accepted layout was evaluated once more before rendering. This draft now computes one `Estimation` per sorted item before the search and merges those cached widths into each column during each attempt, avoiding repeated calls to `GetName()`, `GetSize()`, and owner/group lookup from the search loop. The `TellMe()` rendering path and width rules are unchanged. See the F1 implementation in `main.cc:683–694` and `main.cc:752–772`; the surrounding search is in `main.cc:747–803`, with rendering in `main.cc:371–451`.

### F2 — estimation work repeated by rendering

`UpdateEstimations()` computes name/size widths and, when requested, owner/group names. `TellMe()` computes the corresponding output values again. This supports a field- and mode-dependent duplication claim, but not a blanket claim that a separate directory scan performs all the same work: with prescan enabled, `SingleFile()` first stores the name and metadata; estimation happens later when the collected entries are laid out. The F1 cache now removes repeated estimation across vertical-layout trials, but estimation versus final rendering still occurs separately. See `main.cc:696–733`, `main.cc:752–803`, `main.cc:875–890`, and `main.cc:325–451`.

### F3 — symlink resolution

In `GetName()`, when `Links >= 2` and the sublink path is enabled, `LinkTarget()` is called before and after target inspection. Each call uses `readlink()`. The branch also calls `stat()` and may call `lstat()`—including a color/metadata probe on a successfully followed target. Thus this branch can reach four direct filesystem syscalls for one `GetName()` invocation, but fewer calls are possible depending on path outcome and output budget. This is not a universal count for every listed symlink, and additional code paths (for example size formatting) are separate. See `getname.cc:90–122`, `strfun.cc:38–54`, and `stat.h:30–65`.

### F4 — inode-map insertions

`UpdateEstimations()` inserts every non-directory into `Inodemap`; `Inodemap::insert()` checks only whether the map is enabled and whether the key is already present. There is no `st_nlink > 1` guard, so unique files can incur lookup/insertion work and retain a path in the map. The actual impact depends on whether the map is enabled and the directory contents. See `main.cc:225–266` and `main.cc:696–704`.

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

The vertical layout now builds an `Estimation` vector once in directory order and merges those per-item widths into the candidate column maxima during the binary search. This retains the existing `Inodemap` insertion order and leaves the rendering logic unchanged. No CLI option, default, output format, sorting, palette, or paging behavior was intentionally changed.

## Validation and limitations

For the F1 candidate, the local Release build and the Debug ASan/UBSan build each passed all 15 CTest tests. These supplementary builds used CMake 4.4.4 and GCC 12.2.0; the latter is below the requested GCC 14 floor, so this does not replace the GCC 14+ CI matrix.

Byte comparisons against the baseline were identical for aliases `-a0` through `-a6` on a fixture containing hidden files, a directory, a hardlink, and valid, absolute, and dangling symlinks. A separate `-C -vc` vertical-layout output comparison was also byte-identical.

A local 6,000-entry vertical-layout microbenchmark (3 warm-ups, 15 alternating timed runs per version, same host) measured median wall time of 43.4 ms for baseline and 29.0 ms for the F1 candidate (33.3% lower). This is indicative only: the samples include scheduler noise, and this fixture/timing method is not a substitute for the CI profiler.

`scripts/profile-hot-path.sh` was run with baseline and candidate binaries, but its local report marked Hyperfine, perf, strace, and Heaptrack as `NOT RUN` because those tools are not installed. The PR's GitHub profiling workflow is therefore still needed for its baseline-versus-candidate probe. CMake was installed outside the repository for local validation; no project dependency or workflow was added or changed.
