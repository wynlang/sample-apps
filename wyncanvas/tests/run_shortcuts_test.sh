#!/bin/bash
# Keyboard shortcuts, driven through the REAL editor binary.
#
# WHY THIS IS A SHELL TEST AND NOT A test_*.wyn FILE. Every other suite here
# imports a MODULE and calls it. Shortcut dispatch lives in src/ui.wyn, which is
# the application - it has a `main`, it opens a window, and it cannot be imported.
# So the only way to test it is to run it, which is what WYNCANVAS_SCRIPT plus
# WYNCANVAS_FRAMES exist for: they drive the real widgets and the real event
# queue headlessly under SDL_VIDEODRIVER=dummy.
#
# WHAT IT ASSERTS. That a key produces the STATE CHANGE its button produces -
# read back from the editor's own status line, not inferred. A test that only
# checked "the editor did not crash" would pass while every shortcut did nothing,
# which is exactly the condition this feature was added to end.
#
# The checks are chosen as the places the dispatch table can be WRONG rather than
# absent:
#   - a bare letter picks a paint tool, a DIFFERENT bare letter picks a marquee
#     (the two are mutually exclusive modes, so one must clear the other)
#   - Cmd+A and Cmd+D are opposites and must not collapse to one branch
#   - Cmd+Z and Cmd+Shift+Z must differ (undo vs redo) - the shift bit is the
#     only thing telling them apart, and dropping it is the likeliest bug
#   - the bracket keys must move the radius in OPPOSITE directions
#   - a bare letter that is ALSO a Cmd shortcut (i = eyedropper, Cmd+I = invert
#     selection) must not fire both
#
# WHAT IT CANNOT REACH, stated so nobody reads more into a green run than is
# there. The `key:` action calls handle_shortcut() directly, so this file proves
# the dispatch TABLE and not the two lines that connect the table to the event
# loop (drain_shortcuts' walk over Ui_shortcut_count/at, called from the frame
# loop). Deleting the body of that walk leaves this suite green - measured, not
# assumed. Those two lines are covered from the other side, in repos/gui's
# tests/test_shortcuts.wyn, which drives real Win_push_key_down events through
# Ui_events and asserts the toolkit's half of the partition. The untested seam is
# small and typed; the alternative - pushing synthetic keys here - tests the
# WRONG branch, because a synthetic key does not update the polled modifier state
# under SDL_VIDEODRIVER=dummy, so every chord would arrive as a bare letter.
#
#   ./tests/run_shortcuts_test.sh          # from the repo root
set -uo pipefail
cd "$(dirname "$0")/.."

# Compiler lookup (SKIPs cleanly when there is none) and the bounded runner that
# makes a hang impossible - see tests/lib_ui_test.sh for why both are shared.
. tests/lib_ui_test.sh
ui_test_init

PASS=0
FAIL=0

# Run the editor with a script and echo its shortcut trace.
#
# ui_run REMOVES the generated binary first. `wyn run` caches the executable next
# to the source as src/ui.wyn.out, and a stale one silently ignores every edit to
# ui.wyn or to an imported module - which made five mutation tests falsely
# "survive" while this feature was being written. Deleting it is not paranoia; it
# is the difference between testing this build and testing an old one.
run_script() {
    ui_run 3 "$1" | grep -E '^  key |^  TIMEOUT'
}

# check <label> <haystack> <needle>
check() {
    if printf '%s' "$2" | grep -qF -- "$3"; then
        echo "  ok    $1"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  $1"
        echo "        wanted to find: [$3]"
        echo "        in:"
        printf '%s\n' "$2" | sed 's/^/          /'
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Running keyboard-shortcut test ==="

# ---- tools: bare letters, and the paint/marquee modes are exclusive ----------
OUT=$(run_script 'key:b,key:e,key:g,key:i,key:h,key:m,key:l,key:t')
check "b selects the brush"        "$OUT" "key b -> tool=brush"
check "e selects the eraser"       "$OUT" "key e -> tool=eraser"
check "g selects the bucket fill"  "$OUT" "key g -> tool=fill"
check "i selects the eyedropper"   "$OUT" "key i -> tool=pick"
check "h selects pan"              "$OUT" "key h -> tool=pan"
check "m selects the rect marquee" "$OUT" "sel=rect"
check "l selects the lasso"        "$OUT" "sel=lasso"
check "t selects the text tool"    "$OUT" "sel=text"

# ---- the two selection commands are opposites -------------------------------
OUT=$(run_script 'key:a+cmd,key:d+cmd')
check "cmd+a selects all"          "$OUT" "msg=selected all"
check "cmd+d deselects"            "$OUT" "msg=deselected"

# ---- undo and redo are told apart ONLY by the shift bit ---------------------
# On a fresh document both are empty, so the assertion is which MESSAGE comes
# back: "nothing to undo" vs "nothing to redo". That is precisely the branch a
# dropped shift bit would collapse, and it is observable without first building
# an edit history.
OUT=$(run_script 'key:z+cmd')
check "cmd+z reaches undo"         "$OUT" "msg=nothing to undo"
OUT=$(run_script 'key:z+cmd+shift')
check "cmd+shift+z reaches redo"   "$OUT" "msg=nothing to redo"

# ---- a bare letter and its Cmd chord are different commands -----------------
# i = eyedropper, Cmd+I = invert selection. If the dispatch checked the letter
# before the modifier, one of these would shadow the other.
OUT=$(run_script 'key:i')
check "bare i is the eyedropper"   "$OUT" "tool=pick"
OUT=$(run_script 'key:a+cmd,key:i+cmd')
check "cmd+i inverts the selection, and does NOT pick the eyedropper" "$OUT" "key i -> tool=brush"

# ---- zoom ------------------------------------------------------------------
OUT=$(run_script 'key:plus+cmd,key:minus+cmd,key:0+cmd,key:1+cmd')
check "cmd+= zooms in"             "$OUT" "msg=zoom 200%"
check "cmd+- zooms back out"       "$OUT" "msg=zoom 100%"
check "cmd+0 fits the document"    "$OUT" "msg=fit"
check "cmd+1 is actual pixels"     "$OUT" "key 1 -> tool=brush sel=none msg=zoom 100%"

# ---- brush size: the brackets must move it in OPPOSITE directions -----------
# Asserted as exact values rather than "it changed": two keys that both grew the
# brush would pass a change-only check.
OUT=$(run_script 'key:lbracket,key:rbracket')
check "[ shrinks the brush"        "$OUT" "key lbracket -> tool=brush sel=none msg=radius 8.0"
check "] grows it back"            "$OUT" "key rbracket -> tool=brush sel=none msg=radius 12.0"

# ---- an unbound key is inert, not a crash ----------------------------------
OUT=$(run_script 'key:zzz')
check "an unknown key name is reported" "$OUT" "unknown key"

# A killed run is a failure even if no assertion above happened to notice.
TIMEOUTS=$(ui_test_timeouts)
if [ "$TIMEOUTS" -gt 0 ]; then
    echo "  FAIL  $TIMEOUTS run(s) were killed for exceeding the time bound"
    FAIL=$((FAIL + TIMEOUTS))
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "shortcuts: $PASS pass, 0 fail"
    exit 0
fi
echo "shortcuts: $PASS pass, $FAIL fail"
exit 1
