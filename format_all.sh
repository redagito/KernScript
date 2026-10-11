#!/bin/bash
#
# Format all C++ sources with clang-format using the repo .clang-format
# (LLVM-based, 120 column limit). Run from the repository root.
#
# Tested with clang-format 21.1.7. NOTE: .clang-format sets
# `Standard: Latest`, which tracks the newest standard the installed
# clang-format supports, so a newer clang-format may reformat lines
# that 21.1.7 leaves alone. If --check fails right after a toolchain
# upgrade, re-run the in-place mode with clang-format 21.1.7 before
# assuming the tree is dirty.
#
# Usage:
#   ./format_all.sh          reformat files in place
#   ./format_all.sh --check  dry run: report files needing formatting,
#                            exit 1 when any differ, change nothing

CHECK_MODE=0
if [ "${1:-}" = "--check" ]; then
  CHECK_MODE=1
elif [ "$#" -gt 0 ]; then
  echo "Usage: $0 [--check]" >&2
  exit 2
fi

FAILED=0
for DIRECTORY in Lib Test
do
  if [ "$CHECK_MODE" -eq 1 ]; then
    echo "Checking formatting in $DIRECTORY/"
    if ! find "$DIRECTORY" \( -name '*.h' -or -name '*.cpp' -or -name '*.cxx' -or -name '*.hpp' -or -name '*.hxx' \) -print0 | xargs -0 clang-format --dry-run --Werror --style=file; then
      FAILED=1
    fi
  else
    echo "Formatting code in $DIRECTORY/"
    find "$DIRECTORY" \( -name '*.h' -or -name '*.cpp' -or -name '*.cxx' -or -name '*.hpp' -or -name '*.hxx' \) -print0 | xargs -0 clang-format -i --style=file
  fi
done

if [ "$CHECK_MODE" -eq 1 ]; then
  if [ "$FAILED" -ne 0 ]; then
    echo "Formatting check failed: files above need clang-format." >&2
    exit 1
  fi
  echo "Formatting check passed."
fi
