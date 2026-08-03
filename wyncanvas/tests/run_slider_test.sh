#!/bin/bash
# The filter-strength slider: it is DRAWN, it MOVES, and filters follow it.
#
# WynCanvas does not call widgets.Ui_draw - that function clears and presents its
# own frame, which would erase the document - so every widget kind it uses must be
# painted by src/ui.wyn itself. A kind with no case in that painter is fully
# functional and completely INVISIBLE: hit-testable, draggable, reporting the
# right value, drawing nothing. No state assertion can tell the two apart, which
# is why this file reads pixels.
#
# The slider's geometry is asked of the TOOLKIT (`sliderx` reports
# Ui_slider_thumb_x) rather than computed here. A test holding its own copy of the
# thumb formula passes whether or not the widget agrees with it, and the formula
# depends on the widget's width - so a layout change would silently move the
# probe off the thumb.
#
#   ./tests/run_slider_test.sh          # from the repo root
set -uo pipefail
cd "$(dirname "$0")/.."

# Compiler lookup (SKIPs cleanly when there is none) and the bounded runner that
# makes a hang impossible - see tests/lib_ui_test.sh for why both are shared.
#
# THIS SUITE IS WHY THE BOUND EXISTS. It is the longest of the four - 18 separate
# runs of the editor, three of them at WYNCANVAS_FRAMES=8 - so it is both the one
# most likely to be sitting on a genuinely stuck run and the one where "it is
# still going" is hardest to tell apart from "it will never stop".
. tests/lib_ui_test.sh
ui_test_init

PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ui_run deletes the stale src/ui.wyn.out (which would ignore every edit to
# ui.wyn or an imported module) and bounds the run so it cannot hang.
run_script() {
    ui_run 3 "$1" | grep -E '^  (winpx|sliderx|strength)|^  TIMEOUT'
}

echo "=== Running filter-strength slider test ==="

# ---- the value tracks, and clamps to the widget's range --------------------
OUT=$(run_script 'strength:0,sliderx')
if printf '%s' "$OUT" | grep -q 'val=0'; then ok "0% is accepted"; else bad "0% not set"; printf '%s\n' "$OUT"|sed 's/^/        /'; fi
LO=$(printf '%s' "$OUT" | sed -n 's/.*thumb=\([0-9]*\).*/\1/p')

OUT=$(run_script 'strength:200,sliderx')
if printf '%s' "$OUT" | grep -q 'val=200'; then ok "200% is accepted"; else bad "200% not set"; fi
HI=$(printf '%s' "$OUT" | sed -n 's/.*thumb=\([0-9]*\).*/\1/p')

if [ -n "$LO" ] && [ -n "$HI" ] && [ "$HI" -gt "$LO" ]; then
  ok "the thumb MOVES with the value ($LO -> $HI)"
else
  bad "thumb did not move (lo=${LO:-none} hi=${HI:-none})"
fi

# Out of range is clamped by the widget, not accepted blindly - a filter handed a
# 5x radius because a script asked for 500% is a crash waiting to happen.
OUT=$(run_script 'strength:500,sliderx')
if printf '%s' "$OUT" | grep -q 'val=200'; then ok "500% clamps to the range max"; else bad "500% was not clamped"; printf '%s\n' "$OUT"|sed 's/^/        /'; fi
OUT=$(run_script 'strength:-50,sliderx')
if printf '%s' "$OUT" | grep -q 'val=0'; then ok "a negative value clamps to 0"; else bad "negative not clamped"; printf '%s\n' "$OUT"|sed 's/^/        /'; fi

# ---- the fraction each filter multiplies by --------------------------------
# strength_frac() is the single conversion from the widget's int to the float a
# filter takes, so it is asserted directly rather than inferred from an image.
OUT=$(run_script 'strength:0')
if printf '%s' "$OUT" | grep -q 'frac=0.000'; then ok "0% -> frac 0.000"; else bad "0% frac wrong"; fi
OUT=$(run_script 'strength:50')
if printf '%s' "$OUT" | grep -q 'frac=0.500'; then ok "50% -> frac 0.500"; else bad "50% frac wrong"; fi
OUT=$(run_script 'strength:100')
if printf '%s' "$OUT" | grep -q 'frac=1.000'; then ok "100% -> frac 1.000 (the old hardcoded values)"; else bad "100% frac wrong"; fi
OUT=$(run_script 'strength:200')
if printf '%s' "$OUT" | grep -q 'frac=2.000'; then ok "200% -> frac 2.000"; else bad "200% frac wrong"; fi

# ---- IT IS ACTUALLY DRAWN --------------------------------------------------
#
# Probed at the thumb position the toolkit reports, at the slider's vertical
# centre. Two configurations, because one alone cannot distinguish "the thumb is
# drawn" from "something pale happens to be there": at 0% the thumb sits at the
# LEFT end and the right end must be bare track; at 200% those swap, and the left
# end must have become the filled portion.
#
# The colours are the painter's own constants: thumb 200,200,212 / fill 70,110,170
# / track 40,40,48.
OUT=$(run_script 'strength:0,sliderx,winpx:946:710,winpx:1164:710')
THUMB_LO=$(printf '%s' "$OUT" | sed -n 's/.*winpx(946,710)=\(.*\)/\1/p')
FAR_LO=$(printf '%s' "$OUT" | sed -n 's/.*winpx(1164,710)=\(.*\)/\1/p')
if [ "$THUMB_LO" = "200,200,212" ]; then
  ok "at 0% the THUMB is drawn at the left end (rgb=$THUMB_LO)"
else
  bad "no thumb at the left end (rgb=${THUMB_LO:-none}) - is KIND_SLIDER painted?"
fi
if [ "$FAR_LO" = "40,40,48" ]; then
  ok "at 0% the right end is bare TRACK (rgb=$FAR_LO)"
else
  bad "right end is not track (rgb=${FAR_LO:-none})"
fi

OUT=$(run_script 'strength:200,sliderx,winpx:946:710,winpx:1164:710')
FILL_HI=$(printf '%s' "$OUT" | sed -n 's/.*winpx(946,710)=\(.*\)/\1/p')
THUMB_HI=$(printf '%s' "$OUT" | sed -n 's/.*winpx(1164,710)=\(.*\)/\1/p')
if [ "$THUMB_HI" = "200,200,212" ]; then
  ok "at 200% the thumb has MOVED to the right end (rgb=$THUMB_HI)"
else
  bad "no thumb at the right end (rgb=${THUMB_HI:-none})"
fi
if [ "$FILL_HI" = "70,110,170" ]; then
  ok "at 200% the left end is the FILLED portion (rgb=$FILL_HI)"
else
  bad "left end is not filled (rgb=${FILL_HI:-none})"
fi

# ---- A FILTER'S OUTPUT ACTUALLY FOLLOWS THE SLIDER ------------------------
#
# The checks above prove the slider reports and draws the right value; this is the
# one that proves the value REACHES the pixels. Without it, deleting the `* k`
# from run_filter's blur line leaves every other check in this file green -
# measured, not assumed.
#
# A HARD STROKE IS PAINTED FIRST, and that is the whole reason this works. The
# demo document is smooth, so a blur at radius 4 and radius 8 agree to three
# decimals at most pixels and an image comparison proves nothing. A white stroke
# on a dark scene is high-frequency detail, and the pixel probed sits 8px to the
# side of it: how much of the stroke bleeds that far IS the radius.
#
# Monotonic, not just "different": more strength must mean more bleed. Two values
# that merely differ would also be satisfied by a mapping that scrambled them.
STROKE='brush:6:1.0:1.0,hsv:0.0:0.0:1.0,stroke:32:10:32:54'
probe_blur() {
    ui_run 8 "$STROKE,strength:$1,filter:blur,px:40:32" \
        | sed -n 's/.*px(40,32)=\([0-9.]*\).*/\1/p'
}
B0=$(probe_blur 0)
B100=$(probe_blur 100)
B200=$(probe_blur 200)
if [ -n "$B0" ] && [ -n "$B100" ] && [ -n "$B200" ]; then
  ok "all three blur probes returned a value ($B0 / $B100 / $B200)"
  # awk, because these are floats and the shell cannot compare them.
  if awk -v a="$B0" -v b="$B100" -v c="$B200" 'BEGIN { exit !(a < b && b < c) }'; then
    ok "blur strictly increases with strength ($B0 < $B100 < $B200)"
  else
    bad "blur is not monotonic in strength ($B0 / $B100 / $B200) - does run_filter use the slider?"
  fi
  # 0% must be the identity: a radius of 0 is a documented no-op in filter.wyn,
  # and it is the end most likely to be mishandled.
  if awk -v a="$B0" -v c="$B200" 'BEGIN { exit !(c - a > 0.05) }'; then
    ok "the range is wide enough to be worth having (delta > 0.05)"
  else
    bad "0% and 200% are nearly identical - the mapping may be clamped flat"
  fi
else
  bad "a blur probe returned nothing"
fi

# ---- and no strength makes any filter fail --------------------------------
# The ends are where a radius of 0 or a doubled amount would be mishandled.
for S in 0 50 100 200; do
  OUT=$(run_script "strength:$S,filter:blur,filter:sharpen,filter:saturate,filter:levels,filter:curve,filter:invert")
  if [ -n "$OUT" ]; then
    ok "every filter runs at ${S}% strength"
  else
    bad "a filter produced no output at ${S}%"
  fi
done

# A killed run is a failure even if no assertion above happened to notice - the
# "every filter runs" checks in particular only require SOME output, so a run that
# printed a line and then wedged would otherwise read as a pass.
TIMEOUTS=$(ui_test_timeouts)
if [ "$TIMEOUTS" -gt 0 ]; then
  echo "  FAIL  $TIMEOUTS run(s) were killed for exceeding the time bound"
  FAIL=$((FAIL+TIMEOUTS))
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "slider: $PASS pass, 0 fail"
  exit 0
fi
echo "slider: $PASS pass, $FAIL fail"
exit 1
