#!/bin/bash
# Measure WynJS against node, per workload shape.
#
# Why this exists: ROADMAP OPEN-G claimed WynJS was "~140x slower than node" and
# blamed re-parsing function bodies from source on every call. That number had no
# committed benchmark behind it, so it could be neither reproduced nor used to
# decide anything. This script is the reproduction.
#
# The shapes are chosen to SEPARATE costs, not to produce one number:
#
#   loop_arith    a bare for-loop doing arithmetic. Calls no function, so it
#                 isolates pure interpreter + re-lex-the-loop-body overhead.
#   fn_call       the same loop, but the arithmetic is in a function. The delta
#                 against loop_arith is the cost of a CALL.
#   array_hof     map+filter over a large array: the callfn2 two-arg fast path.
#   string_build  repeated string concatenation (runtime string cost).
#   obj_prop      object property get/set in a loop.
#   fib           recursion, so call overhead compounds with depth.
#
# Each shape is run REPS times and the FASTEST run is reported. Fastest, not
# mean: this box is shared with other agents, so noise is one-sided (a run can
# be slowed by a neighbour, never sped up), and the minimum is the closest
# estimate of the true cost. Check `uptime` before trusting small deltas.
#
# Usage:
#   bash tests/run_bench.sh                 # build from src, then measure
#   WYNJS=/path/to/binary bash tests/run_bench.sh   # measure an existing binary
#   REPS=5 bash tests/run_bench.sh
#
# Honours $WYN (compiler to build with) and $TMPDIR, like the other harnesses.
set -uo pipefail
cd "$(dirname "$0")/.."

REPS="${REPS:-3}"
WYN="${WYN:-../../wyn/wyn}"
TMP="${TMPDIR:-/tmp}"
[ "${TMP%/}" = "$TMP" ] && TMP="$TMP/"

BENCHDIR=tests/bench

# --release, not the default build. `wyn build` defaults to -O0; the C compiler
# then leaves the interpreter loop unoptimised, which measures the debug build
# rather than the product. Anything reported to a user must be the -O3 build.
if [ -n "${WYNJS:-}" ]; then
    BIN="$WYNJS"
    [ -x "$BIN" ] || { echo "WYNJS=$BIN is not executable"; exit 1; }
    echo "binary:   $BIN (pre-built, given via \$WYNJS)"
else
    BIN="${TMP}wynjs_bench_bin.$$"
    rm -f "$BIN"
    if ! "$WYN" build src/main.wyn --release -o "$BIN" >/dev/null 2>&1; then
        echo "BUILD FAILED"; "$WYN" build src/main.wyn --release -o "$BIN" 2>&1 | tail -20; exit 1
    fi
    echo "binary:   $BIN (built --release with $WYN)"
    trap 'rm -f "$BIN"' EXIT
fi

HAVE_NODE=0
if command -v node >/dev/null 2>&1; then HAVE_NODE=1; NODEV=$(node --version); else NODEV="(absent)"; fi

echo "node:     $NODEV"
echo "reps:     $REPS (fastest run reported)"
echo "load:     $(uptime | sed 's/.*load averages*: //')"
echo ""

# Fastest wall-clock of $REPS runs, in seconds, to 3dp. Uses bash's SECONDS-free
# python3 timer because `time` on this box reports only 2dp and the fast node
# runs need more resolution than that.
best_of() {
    python3 - "$REPS" "$@" <<'PY'
import subprocess, sys, time
reps = int(sys.argv[1]); cmd = sys.argv[2:]
best = None
for _ in range(reps):
    t = time.perf_counter()
    r = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = time.perf_counter() - t
    if r.returncode != 0:
        print("ERR"); sys.exit(0)
    if best is None or d < best: best = d
print("%.3f" % best)
PY
}

printf "%-14s %10s %10s %8s\n" shape wynjs node ratio
printf "%-14s %10s %10s %8s\n" -------------- ---------- ---------- --------

TOTAL_W=0; TOTAL_N=0
for js in "$BENCHDIR"/*.js; do
    name=$(basename "$js" .js)
    w=$(best_of "$BIN" "$js")
    if [ "$HAVE_NODE" = 1 ]; then n=$(best_of node "$js"); else n="-"; fi

    if [ "$w" = "ERR" ] || [ "$n" = "ERR" ]; then
        printf "%-14s %10s %10s %8s\n" "$name" "$w" "$n" "ERR"
        continue
    fi
    if [ "$HAVE_NODE" = 1 ]; then
        ratio=$(python3 -c "print('%.0fx' % ($w/$n))" 2>/dev/null || echo "-")
        TOTAL_W=$(python3 -c "print($TOTAL_W+$w)")
        TOTAL_N=$(python3 -c "print($TOTAL_N+$n)")
    else
        ratio="-"
    fi
    printf "%-14s %10s %10s %8s\n" "$name" "$w" "$n" "$ratio"
done

if [ "$HAVE_NODE" = 1 ]; then
    echo ""
    printf "%-14s %10.3f %10.3f %7.0fx\n" TOTAL "$TOTAL_W" "$TOTAL_N" \
        "$(python3 -c "print($TOTAL_W/$TOTAL_N)")"
    echo ""
    echo "NOTE: the ratio is dominated by node's ~40ms process floor on the fast"
    echo "shapes. A ratio is only meaningful next to the absolute times."
fi
