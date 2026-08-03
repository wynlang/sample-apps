# Shared plumbing for the four run_*_test.sh suites. Sourced, never executed.
#
# These suites all do the same two awkward things: find the compiler, and run the
# real editor headlessly. Both have a failure mode that is worse than a red test.
#
# 1. FINDING THE COMPILER UNDER `set -u`.
#    Each script wants to SKIP when there is no compiler to point at. Written the
#    obvious way - `WYN="${WYN:-$WYN_ROOT/wyn}"` - it does the opposite: under
#    `set -u` the expansion of an unset WYN_ROOT aborts the script with
#    "WYN_ROOT: unbound variable" BEFORE the `if [ ! -x "$WYN" ]` guard below it
#    can run, so the deliberate SKIP path is unreachable and a machine with no
#    compiler reports a hard error instead. ui_test_init resolves both names with
#    an explicit `:-` default each, which is the one construct `set -u` permits,
#    so the guard is reached. `set -u` stays on for every other name.
#
# 2. RUNNING THE EDITOR WITHOUT RISKING A HANG.
#    src/ui.wyn's frame loop is `while Ui_running(ui)` and only breaks after
#    WYNCANVAS_FRAMES frames. With that variable unset - or dropped by a typo, or
#    ignored because the run died before parsing it - the loop never ends and the
#    suite hangs forever, which in CI is indistinguishable from a wedged runner
#    and locally is a test run you have to notice and kill by hand.
#
#    So no invocation is unbounded. ui_run enforces a per-invocation limit AND a
#    whole-script budget, and a suite can therefore only end one of three ways:
#    pass, fail, or skip. There is no fourth.
#
#    The bound is shell-native on purpose: there is no `timeout` binary on macOS
#    (it is GNU coreutils, not POSIX), so anything built on it would be
#    Linux-only. A background job plus a polling counter works anywhere bash does.
#
#    The kill is sent to the PROCESS GROUP, not the pid. `wyn run` compiles to
#    src/ui.wyn.out and execs it as a child, so killing the pid alone would leave
#    the editor running and holding the window - the group kill (which `set -m`
#    makes possible, by putting the job in its own group) reaps both.

# Per-invocation ceiling, and the budget for the whole script. Typical invocation
# is ~3s and the slowest suite is ~45s end to end, so these are ~10x headroom -
# loose enough never to fire on a slow or loaded machine, tight enough that a
# genuinely stuck run is caught in minutes rather than never. Both overridable.
UI_TEST_TIMEOUT="${UI_TEST_TIMEOUT:-90}"
UI_TEST_BUDGET="${UI_TEST_BUDGET:-600}"

# Resolve the compiler, or SKIP. Call once, from the package root.
#
# Exports WYN. Prints the SKIP line and exits 0 - not 1 - when there is nothing
# to run: "this machine has no compiler" is not a test failure, and reporting it
# as one makes a green tree impossible on any checkout without one.
ui_test_init() {
    WYN="${WYN:-}"
    if [ -z "$WYN" ]; then
        _wyn_root="${WYN_ROOT:-}"
        [ -n "$_wyn_root" ] && WYN="$_wyn_root/wyn"
    fi
    if [ -z "$WYN" ] || [ ! -x "$WYN" ]; then
        echo "SKIP: no wyn binary (set WYN or WYN_ROOT)"
        exit 0
    fi

    UI_TEST_START=$(date +%s)
    # An explicit XXXXXX template, because `mktemp -t prefix` means different
    # things in the two implementations: BSD/macOS treats the argument as a
    # prefix, GNU coreutils treats it as a template and rejects it outright
    # ("too few X's in template"). The template form is accepted by both.
    UI_TEST_OUT=$(mktemp "${TMPDIR:-/tmp}/wyncanvas_ui_out.XXXXXX")
    # The timeout tally lives in a FILE because every call site captures output
    # with $(...), which runs in a subshell - a shell variable incremented there
    # is discarded when the subshell exits, so a hung invocation would leave no
    # trace in the summary.
    UI_TEST_TIMEOUTS=$(mktemp "${TMPDIR:-/tmp}/wyncanvas_ui_to.XXXXXX")
    printf '0' > "$UI_TEST_TIMEOUTS"
    trap 'rm -f "$UI_TEST_OUT" "$UI_TEST_TIMEOUTS"' EXIT
}

# ui_bounded <seconds> <command> [args...]
#
# Runs the command with stdout+stderr on this function's stdout, killed if it
# outlasts <seconds>. Returns the command's status, or 124 on expiry.
ui_bounded() {
    _secs="$1"; shift

    # Whatever is left of the whole-script budget, so a suite cannot be dragged
    # out by many invocations that are each individually under the ceiling.
    _left=$(( UI_TEST_BUDGET - ( $(date +%s) - UI_TEST_START ) ))
    if [ "$_left" -le 0 ]; then
        echo "  TIMEOUT: script budget of ${UI_TEST_BUDGET}s exhausted"
        printf '%s' "$(( $(cat "$UI_TEST_TIMEOUTS") + 1 ))" > "$UI_TEST_TIMEOUTS"
        return 124
    fi
    [ "$_left" -lt "$_secs" ] && _secs="$_left"

    # Own process group, so the kill below reaches the compiled editor too.
    case "$-" in *m*) _had_monitor=1 ;; *) _had_monitor=0 ;; esac
    set -m
    # stdin from /dev/null: a run that decides to read a line must fail fast
    # rather than block on an inherited terminal and then be killed as a timeout,
    # which would report the wrong cause.
    ( "$@" ) > "$UI_TEST_OUT" 2>&1 < /dev/null &
    _pid=$!
    [ "$_had_monitor" = 1 ] || set +m

    _waited=0
    _limit=$(( _secs * 5 ))          # polled every 0.2s
    _timed_out=0
    while kill -0 "$_pid" 2>/dev/null; do
        if [ "$_waited" -ge "$_limit" ]; then
            kill -9 -"$_pid" 2>/dev/null || kill -9 "$_pid" 2>/dev/null
            _timed_out=1
            break
        fi
        sleep 0.2
        _waited=$(( _waited + 1 ))
    done
    wait "$_pid" 2>/dev/null
    _rc=$?

    cat "$UI_TEST_OUT"
    if [ "$_timed_out" = 1 ]; then
        # Loud, and on stdout with the captured output, so it lands in the
        # diagnostic block a failing check prints instead of vanishing.
        echo "  TIMEOUT: killed after ${_secs}s"
        printf '%s' "$(( $(cat "$UI_TEST_TIMEOUTS") + 1 ))" > "$UI_TEST_TIMEOUTS"
        return 124
    fi
    return "$_rc"
}

# ui_run <frames> <script>
#
# One headless run of the editor, bounded. SDL_VIDEODRIVER=dummy is what makes it
# headless - it is a capability switch, not an OS check, so it behaves the same
# on a Linux box with no X display as on a Mac with a desktop.
#
# The cached binary is REMOVED first. `wyn run` caches next to the source as
# src/ui.wyn.out, and a stale one silently ignores every edit to ui.wyn or an
# imported module - which made five mutation tests falsely "survive" while these
# features were being written.
ui_run() {
    rm -f src/ui.wyn.out src/ui.wyn.c
    ui_bounded "$UI_TEST_TIMEOUT" \
        env SDL_VIDEODRIVER=dummy WYNCANVAS_FRAMES="$1" WYNCANVAS_SCRIPT="$2" \
            "$WYN" run src/ui.wyn
}

# How many invocations were killed. A suite adds this to its own failure count:
# a run that hung is not a pass, however few assertions noticed.
ui_test_timeouts() { cat "$UI_TEST_TIMEOUTS"; }
