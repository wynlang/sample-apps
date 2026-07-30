#!/bin/bash
# Cross-check WynJS against the REFERENCE implementation.
#
# The Wyn-side suite (tests/test_main.wyn) compares WynJS against .out files, so it
# proves WynJS is SELF-CONSISTENT. That is not the same as being correct: a .out
# file written by looking at WynJS's own output will happily enshrine a bug. Exactly
# that happened - `parseInt("abc")` was recorded as 0, and node says NaN. The suite
# passed 20/20 while the interpreter was wrong.
#
# So this script diffs each case against `node` when node is available. It is the
# only test here that can discover a wrong EXPECTATION rather than a wrong result.
#
# Skips cleanly when node is absent (CI without node still runs the Wyn suite).
#
# Three cases are expected to differ and are listed with reasons; anything else
# differing is a real finding.
set -uo pipefail
cd "$(dirname "$0")/.."

if ! command -v node >/dev/null 2>&1; then
    echo "SKIP: node not installed - cannot check parity with the reference"
    exit 0
fi

echo "node $(node --version)"
PASS=0; DIFF=0; EXPECTED=0

# Cases that legitimately differ from node, with the reason. Keep this list SHORT
# and justified - it is the place where "we are different" hides.
is_expected_diff() {
    case "$1" in
        # node prints a multi-line stack trace with absolute paths; WynJS prints a
        # single-line message. A trace is not a semantic difference, and matching
        # node's exactly would mean embedding this machine's paths in a .out file.
        uncaught_throw) return 0 ;;
        # Promise ordering/timing and Math precision at the extremes: node's exact
        # float formatting and microtask interleaving are not a goal for a teaching
        # interpreter. Documented rather than silently ignored.
        promises_math|limits_overflow) return 0 ;;
        *) return 1 ;;
    esac
}

for js in tests/cases/*.js; do
    c=$(basename "$js" .js)
    exp="tests/cases/$c.out"
    [ -f "$exp" ] || { echo "  ??    $c (no .out file)"; DIFF=$((DIFF+1)); continue; }
    # Strip node's ANSI colouring, which is presentation, not output.
    if diff -q <(node "$js" 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g') "$exp" >/dev/null 2>&1; then
        echo "  ok    $c"
        PASS=$((PASS+1))
    elif is_expected_diff "$c"; then
        echo "  ~     $c (known, justified difference)"
        EXPECTED=$((EXPECTED+1))
    else
        echo "  DIFF  $c"
        diff <(node "$js" 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g') "$exp" | head -6 | sed 's/^/          /'
        DIFF=$((DIFF+1))
    fi
done

echo ""
echo "node parity: $PASS identical, $EXPECTED known-different, $DIFF unexplained"
[ "$DIFF" -eq 0 ]
