#!/usr/bin/env bash
# Informational same-host profiling for baseline vs candidate dirr binaries.
# Tool/permission failures are recorded as NOT RUN and do not fail the script.
set -u -o pipefail

usage() {
    cat >&2 <<'EOF'
Usage: profile-hot-path.sh BASELINE_DIRR DRAFT_DIRR [OUTPUT_DIR]

Environment overrides:
  DIRR_PROFILE_LINKS       Number of symlinks in the generated fixture (default 10000)
  DIRR_PROFILE_WARMUPS     Hyperfine warm-ups (default 10)
  DIRR_PROFILE_RUNS         Hyperfine measured runs (default 50)
  DIRR_PROFILE_PERF_RUNS    perf stat repetitions (default 10)
EOF
}

if (( $# < 2 || $# > 3 )); then
    usage
    exit 2
fi

BASELINE=$1
DRAFT=$2
OUT=${3:-"${TMPDIR:-/tmp}/dirr-profile-$(date +%Y%m%d-%H%M%S)"}
LINK_COUNT=${DIRR_PROFILE_LINKS:-10000}
WARMUPS=${DIRR_PROFILE_WARMUPS:-10}
RUNS=${DIRR_PROFILE_RUNS:-50}
PERF_RUNS=${DIRR_PROFILE_PERF_RUNS:-10}

for value in "$LINK_COUNT" "$WARMUPS" "$RUNS" "$PERF_RUNS"; do
    if [[ ! $value =~ ^[0-9]+$ ]]; then
        echo "Expected a non-negative integer, got: $value" >&2
        exit 2
    fi
done

for bin in "$BASELINE" "$DRAFT"; do
    if [[ ! -x $bin ]]; then
        echo "Not an executable file: $bin" >&2
        exit 2
    fi
done

mkdir -p "$OUT"
SUMMARY="$OUT/summary.txt"
: > "$SUMMARY"
record() {
    printf '%s\n' "$*" | tee -a "$SUMMARY"
}

# Ignore per-user dirr defaults so every profiled invocation has the same options.
unset DIRR DIRR_COLORS || true
export LC_ALL=C TZ=UTC COLS=80 COLUMNS=80

FIXTURE=$(mktemp -d "${TMPDIR:-/tmp}/dirr-profile-fixture.XXXXXX") || exit 2
trap 'rm -rf -- "$FIXTURE"' EXIT
mkdir -p "$FIXTURE/targets" "$FIXTURE/links"
printf 'dirr profiling target\n' > "$FIXTURE/targets/file"
for ((i = 1; i <= LINK_COUNT; ++i)); do
    printf -v name 'link-%05d' "$i"
    ln -s ../targets/file "$FIXTURE/links/$name"
done
LINKS="$FIXTURE/links"

record "baseline=$BASELINE"
record "draft=$DRAFT"
record "fixture_links=$LINK_COUNT"
record "hyperfine_warmups=$WARMUPS hyperfine_runs=$RUNS"
record "perf_runs=$PERF_RUNS"

run_hyperfine() {
    local mode=$1
    local base_cmd draft_cmd log json
    log="$OUT/hyperfine-l${mode}.txt"
    json="$OUT/hyperfine-l${mode}.json"
    printf -v base_cmd '"%s" -l%s -A -c -X80 "%s" >/dev/null' "$BASELINE" "$mode" "$LINKS"
    printf -v draft_cmd '"%s" -l%s -A -c -X80 "%s" >/dev/null' "$DRAFT" "$mode" "$LINKS"

    if ! command -v hyperfine >/dev/null 2>&1; then
        record "hyperfine -l${mode}: NOT RUN (hyperfine not installed)"
    elif hyperfine --warmup "$WARMUPS" --runs "$RUNS" \
            --export-json "$json" "$base_cmd" "$draft_cmd" >"$log" 2>&1; then
        record "hyperfine -l${mode}: PASS; see $log and $json"
    else
        record "hyperfine -l${mode}: NOT RUN/FAILED; see $log"
    fi
}

run_perf() {
    local label=$1 bin=$2 log err
    log="$OUT/perf-${label}-l0.txt"
    err="$OUT/perf-${label}-l0.stderr"
    if ! command -v perf >/dev/null 2>&1; then
        record "perf stat ${label}: NOT RUN (perf not installed)"
    elif perf stat -r "$PERF_RUNS" \
            -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
            -o "$log" -- "$bin" -l0 -A -c -X80 "$LINKS" \
            >/dev/null 2>"$err"; then
        record "perf stat ${label}: PASS; see $log"
    else
        record "perf stat ${label}: NOT RUN (counter/permission error); see $err"
    fi
}

run_strace() {
    local label=$1 bin=$2 log err
    log="$OUT/strace-${label}-l0.txt"
    err="$OUT/strace-${label}-l0.stderr"
    if ! command -v strace >/dev/null 2>&1; then
        record "strace ${label}: NOT RUN (strace not installed)"
    elif strace -c -e trace=%file,getdents64 -o "$log" -- \
            "$bin" -l0 -A -c -X80 "$LINKS" >/dev/null 2>"$err"; then
        if [[ -s "$log" ]]; then
            record "strace ${label}: PASS; see $log"
        else
            record "strace ${label}: NOT RUN/FAILED (empty syscall summary); see $err"
        fi
    else
        record "strace ${label}: NOT RUN (ptrace/filter error); see $err"
    fi
}

is_dynamically_linked() {
    local bin=$1 program_headers ldd_output
    if command -v readelf >/dev/null 2>&1; then
        if program_headers=$(readelf -l "$bin" 2>/dev/null); then
            [[ $program_headers == *INTERP* ]]
            return
        fi
    fi
    if command -v ldd >/dev/null 2>&1; then
        ldd_output=$(ldd "$bin" 2>&1 || true)
        [[ $ldd_output != *"not a dynamic executable"* && $ldd_output != *"statically linked"* ]]
        return
    fi
    # If no ELF inspection tool is available, attempt Heaptrack and report its result.
    return 0
}

run_heaptrack() {
    local label=$1 bin=$2 output_base log profile
    output_base="$OUT/heaptrack-${label}-l0"
    log="$OUT/heaptrack-${label}-l0.log"
    if ! command -v heaptrack >/dev/null 2>&1; then
        record "heaptrack ${label}: NOT RUN (heaptrack not installed)"
    elif ! is_dynamically_linked "$bin"; then
        record "heaptrack ${label}: NOT RUN (static binary; Heaptrack requires LD_PRELOAD)"
    else
        # Heaptrack appends its own .gz or .zst suffix to --output.
        rm -f -- "${output_base}.gz" "${output_base}.zst" "$log"
        record "heaptrack ${label}: starting -l0; diagnostics: $log"
        if heaptrack --output "$output_base" -- "$bin" -l0 -A -c -X80 "$LINKS" \
                >/dev/null 2>"$log"; then
            if [[ -s "${output_base}.gz" ]]; then
                profile="${output_base}.gz"
            elif [[ -s "${output_base}.zst" ]]; then
                profile="${output_base}.zst"
            else
                record "heaptrack ${label}: NOT RUN/FAILED (no non-empty profile); see $log"
                return
            fi
            record "heaptrack ${label}: PASS; see $profile and $log"
        else
            record "heaptrack ${label}: NOT RUN/FAILED; see $log"
        fi
    fi
}

run_hyperfine 0
run_hyperfine 3
for label in baseline draft; do
    if [[ $label == baseline ]]; then bin=$BASELINE; else bin=$DRAFT; fi
    run_perf "$label" "$bin"
    run_strace "$label" "$bin"
    run_heaptrack "$label" "$bin"
done

record "Results directory: $OUT"
