# Performance audit findings (F1–F11)

**Reviewed revision:** `0495bf12094b059ad3cb08fd100ebdc065d411d5`

**Review type:** source inspection; no runtime performance claims are made.

**Scope:** findings F1–F11 from the supplied audit. F12/PGO is intentionally out of scope at the user's request.

## Summary

| Finding | Assessment |
| --- | --- |
| F1 | Confirmed structurally: vertical-column layout repeats width estimation during binary search. |
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

`PrintAllFilesCollectedSoFar()` uses a binary search to choose the vertical layout. Each `acceptable()` attempt calls `UpdateEstimations()` for the items it visits, and after the search the accepted layout is evaluated once more to repopulate the column estimates. `TellMe()` then computes output fields separately. This makes repeated estimation structurally real in vertical multi-column mode; the number of calls and work per call depend on the chosen layout and fields. See `main.cc:734–785`, `main.cc:683–720`, and `main.cc:371–451`.

### F2 — estimation work repeated by rendering

`UpdateEstimations()` computes name/size widths and, when requested, owner/group names. `TellMe()` computes the corresponding output values again. This supports a field- and mode-dependent duplication claim, but not a blanket claim that a separate directory scan performs all the same work: with prescan enabled, `SingleFile()` first stores the name and metadata; estimation happens later when the collected entries are laid out. Vertical layout can add the F1 repetitions. See `main.cc:683–720`, `main.cc:753–785`, `main.cc:857–872`, and `main.cc:325–451`.

### F3 — symlink resolution

In `GetName()`, when `Links >= 2` and the sublink path is enabled, `LinkTarget()` is called before and after target inspection. Each call uses `readlink()`. The branch also calls `stat()` and may call `lstat()`—including a color/metadata probe on a successfully followed target. Thus this branch can reach four direct filesystem syscalls for one `GetName()` invocation, but fewer calls are possible depending on path outcome and output budget. This is not a universal count for every listed symlink, and additional code paths (for example size formatting) are separate. See `getname.cc:90–122`, `strfun.cc:38–54`, and `stat.h:30–65`.

### F4 — inode-map insertions

`UpdateEstimations()` inserts every non-directory into `Inodemap`; `Inodemap::insert()` checks only whether the map is enabled and whether the key is already present. There is no `st_nlink > 1` guard, so unique files can incur lookup/insertion work and retain a path in the map. The actual impact depends on whether the map is enabled and the directory contents. See `main.cc:225–266` and `main.cc:683–691`.

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

`ScanDir()` builds a full path string for each directory entry before calling `SingleFile()`, and the stat wrappers ultimately call path-based `stat()`/`lstat()`. However, `try_stat(const std::string&)` and `try_lstat(const std::string&)` pass `c_str()` directly; the extra owned string is specific to the `std::string_view` overload. Replacing path-based lookup with `fstatat()` would require a directory-handle-aware design and compatibility checks, not just changing the wrapper. See `main.cc:977–995` and `stat.h:30–65`.

### F11 — DFA mutex

`DFA_DISABLE_MUTEX` is defined under `__SUNPRO_CC`; absent an external build definition, a normal GCC build therefore includes the matcher mutex (or shared-timed mutex). This confirms that synchronization is present, but the source review did not establish that it is a hot path or that removing it would be safe for all uses. Any change should remain narrowly scoped and preserve existing lock semantics. See `dfa_match.hh:6–16`, `dfa_match.hh:140–152`, and the lock macros in `dfa_match.cc:497–505`.

## Validation and limitations

This PR contains documentation only; no source, test, workflow, CLI, or default-output behavior is changed. Runtime validation has **not** been performed:

- Release and ASan/UBSan builds with CTest: not run.
- `scripts/profile-hot-path.sh` baseline-versus-candidate: not run; no baseline/candidate executables were built.
- Byte-for-byte output comparison for `dirr -a0..a6 -A -c`: not run.

The local environment has no `cmake`/`ctest`, and its `g++` is 12.2.0, below the requested GCC 14 validation floor. Profiling tools (`hyperfine`, `perf`, `strace`, and `heaptrack`) are also absent. This report is consequently a static evidence review only; do not treat it as a benchmark or as approval for an optimization patch.
