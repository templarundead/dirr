#!/usr/bin/env sh
set -eu

binary=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/dirr-smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

: > "$work/visible.txt"
: > "$work/.hidden"
mkdir "$work/subdir"
ln -s visible.txt "$work/link"

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

# -l0 follows the symbolic link and exercises the stat() expected wrapper.
link=$("$binary" -l0 -c --format=.f "$work/link")
printf '%s' "$link" | grep -Fq 'link'

# -l3 resolves and displays a relative symlink target in the file's directory.
link_stats=$("$binary" -l3 -c --format=.f_.s "$work/link")
printf '%s' "$link_stats" | grep -Fq 'link ->'
printf '%s' "$link_stats" | grep -Fq 'visible.txt'
printf '%s' "$link_stats" | grep -Fq '0'
