#!/usr/bin/env sh
set -eu

binary=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/dirr-smoke.XXXXXX")
logs=$(mktemp -d "${TMPDIR:-/tmp}/dirr-smoke-logs.XXXXXX")
home=$(mktemp -d "${TMPDIR:-/tmp}/dirr-smoke-home.XXXXXX")
trap 'rm -rf "$work" "$logs" "$home"' EXIT HUP INT TERM
HOME=$home
export HOME

: > "$work/visible.txt"
: > "$work/.hidden"
mkdir "$work/subdir"
ln -s visible.txt "$work/link"

# Help text, including the option list, belongs entirely on stdout.
"$binary" --help >"$logs/help.out" 2>"$logs/help.err"
grep -Fq 'Usage:' "$logs/help.out"
grep -Fq 'Options:' "$logs/help.out"
if [ -s "$logs/help.err" ]; then
    echo 'help unexpectedly wrote to stderr' >&2
    cat "$logs/help.err" >&2
    exit 1
fi

# Invalid options must produce a diagnostic without crashing in the formatter.
if "$binary" --definitely-not-an-option >"$logs/invalid.out" 2>"$logs/invalid.err"; then
    echo 'invalid option unexpectedly succeeded' >&2
    exit 1
fi
if ! grep -Fq 'unrecognized option' "$logs/invalid.out" "$logs/invalid.err"; then
    echo 'invalid option diagnostic was missing' >&2
    exit 1
fi

# A huge link-mode value must be rejected before narrowing to int.
if "$binary" --links=4294967296 >"$logs/links.out" 2>"$logs/links.err"; then
    echo 'out-of-range link mode unexpectedly succeeded' >&2
    exit 1
fi
if ! grep -Fq 'invalid parameter' "$logs/links.out" "$logs/links.err"; then
    echo 'out-of-range link mode diagnostic was missing' >&2
    exit 1
fi

if "$binary" --width=999999999999999999999999999999 >"$logs/width.out" 2>"$logs/width.err"; then
    echo 'out-of-range width unexpectedly succeeded' >&2
    exit 1
fi
if ! grep -Fq 'invalid parameter' "$logs/width.out" "$logs/width.err"; then
    echo 'out-of-range width diagnostic was missing' >&2
    exit 1
fi

visible=$("$binary" -c --format=.f "$work")
printf '%s\n' "$visible"
printf '%s' "$visible" | grep -Fq 'visible.txt'
printf '%s' "$visible" | grep -Fq 'subdir/'
if printf '%s' "$visible" | grep -Fq '.hidden'; then
    echo 'hidden file unexpectedly shown without -A' >&2
    exit 1
fi

all=$("$binary" -A -c --format=.f "$work")
printf '%s\n' "$all"
printf '%s' "$all" | grep -Fq '.hidden'

# Force color-mode initialization to exercise the extension-pattern DFA cache.
"$binary" -c1 --format=.f "$work" >"$logs/cache-create.out"
grep -Fq 'visible.txt' "$logs/cache-create.out"

# Exercise both loading a valid DFA cache and safely regenerating a corrupt one.
if [ ! -s "$home/.dirr_dfa" ]; then
    echo 'DFA cache was not created' >&2
    exit 1
fi
dd if=/dev/zero bs=32 count=1 2>/dev/null | tr '\000' '\377' > "$logs/corrupt.dfa"
cp "$logs/corrupt.dfa" "$home/.dirr_dfa"
"$binary" -c1 --format=.f "$work" >"$logs/cache-recovery.out"
grep -Fq 'visible.txt' "$logs/cache-recovery.out"
if cmp -s "$logs/corrupt.dfa" "$home/.dirr_dfa"; then
    echo 'corrupt DFA cache was not replaced' >&2
    exit 1
fi

# A file path is accepted as a command-line operand and listed by itself.
file=$("$binary" -c --format=.f "$work/visible.txt")
printf '%s' "$file" | grep -Fq 'visible.txt'

# -l0 follows the symbolic link and exercises the stat() expected wrapper.
link=$("$binary" -l0 -c --format=.f "$work/link")
printf '%s' "$link" | grep -Fq 'link'

# -l3 resolves and displays a relative symlink target in the file's directory.
link_stats=$("$binary" -l3 -c --format=.f_.s "$work/link")
printf '%s' "$link_stats" | grep -Fq 'link ->'
printf '%s' "$link_stats" | grep -Fq 'visible.txt'
printf '%s' "$link_stats" | grep -Fq '0'
