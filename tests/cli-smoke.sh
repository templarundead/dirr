#!/usr/bin/env sh
set -eu

binary=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/dirr-smoke.XXXXXX")
logs=$(mktemp -d "${TMPDIR:-/tmp}/dirr-smoke-logs.XXXXXX")
trap 'rm -rf "$work" "$logs"' EXIT HUP INT TERM

: > "$work/visible.txt"
: > "$work/.hidden"
mkdir "$work/subdir"
utf8_name=$(printf 'utf8-\303\251.txt')
: > "$work/$utf8_name"
invalid_name=$(printf 'invalid-\377-name')
: > "$work/$invalid_name"
long_name=$(printf '%*s' 240 '' | tr ' ' L)
: > "$work/$long_name"
ln "$work/visible.txt" "$work/hardlink.txt"
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

# A file path is accepted as a command-line operand and listed by itself.
file=$("$binary" -c --format=.f "$work/visible.txt")
printf '%s' "$file" | grep -Fq 'visible.txt'

# -l0 follows the symbolic link, showing the target's size rather than the
# symlink's own length, and exercises the stat() expected wrapper.
link=$("$binary" -l0 -c --format=.f_.s "$work/link")
printf '%s' "$link" | grep -Fq 'link 0'

# The aliases a2/a3 also select Links == 0 while scanning directory entries.
for alias in 2 3; do
    listing=$("$binary" "-a$alias" -c -X80 "$work")
    printf '%s' "$listing" | grep -Fq 'link'
done

# -l3 resolves and displays a relative symlink target in the file's directory.
link_stats=$("$binary" -l3 -c --format=.f_.s "$work/link")
printf '%s' "$link_stats" | grep -Fq 'link ->'
printf '%s' "$link_stats" | grep -Fq 'visible.txt'
printf '%s' "$link_stats" | grep -Fq '0'

# The octal attribute field keeps the legacy 32-column zero padding and takes
# the requested two through nine trailing digits.
mode_file="$work/mode-file"
: > "$mode_file"
chmod 0644 "$mode_file"
attrs=2
for expected in 44 644 0644 00644 100644 0100644 00100644 000100644; do
    actual=$("$binary" -c -m2 -X80 "--format=.a$attrs" "$mode_file")
    if [ "$actual" != "$expected" ]; then
        echo ".a$attrs produced '$actual', expected '$expected'" >&2
        exit 1
    fi
    attrs=$((attrs + 1))
done

# Special mode bits remain part of the printed octal mode.
chmod 4751 "$mode_file"
actual=$("$binary" -c -m2 -X80 --format=.a9 "$mode_file")
[ "$actual" = "000104751" ] || { echo "setuid mode was '$actual'" >&2; exit 1; }
chmod 2751 "$mode_file"
actual=$("$binary" -c -m2 -X80 --format=.a9 "$mode_file")
[ "$actual" = "000102751" ] || { echo "setgid mode was '$actual'" >&2; exit 1; }
chmod 1751 "$mode_file"
actual=$("$binary" -c -m2 -X80 --format=.a9 "$mode_file")
[ "$actual" = "000101751" ] || { echo "sticky mode was '$actual'" >&2; exit 1; }

# Empty operands, trailing separators, and long directory prefixes must not
# produce invalid path-buffer accesses or lose directory entries.
empty_operand=$("$binary" -m2 -c -X80 "")
[ -n "$empty_operand" ] || { echo 'empty operand produced no diagnostic' >&2; exit 1; }
trailing_listing=$("$binary" -m2 -c -X80 --format=.f "$work/")
printf '%s' "$trailing_listing" | grep -Fq 'visible.txt'

long_dir="$work/long"
mkdir "$long_dir"
segment=$(printf '%*s' 170 '' | tr ' ' x)
for index in 1 2 3 4; do
    long_dir="$long_dir/$segment"
    mkdir "$long_dir"
done
: > "$long_dir/deep-file.txt"
deep_listing=$("$binary" -m2 -c -X80 --format=.f "$long_dir")
printf '%s' "$deep_listing" | grep -Fq 'deep-file.txt'
