#!/bin/bash
# Marching ants: the selection's real SHAPE reaches the screen.
#
# The editor used to draw a bounding BOX for every selection, so an ellipse
# showed the rectangle it was inscribed in. The unit tests in test_select.wyn
# prove the outline KERNEL is right (hand-derived coverage values); this file
# proves the outline is actually VISIBLE, which no state assertion can - geometry
# says nothing about what got blitted.
#
# So it reads PIXELS from a real frame, at a point on an ellipse's boundary that
# is deliberately far from any bounding-box edge. If the ants were still a bbox,
# that point would show the document underneath instead.
#
#   ./tests/run_ants_test.sh          # from the repo root
set -uo pipefail
cd "$(dirname "$0")/.."

WYN="${WYN:-$WYN_ROOT/wyn}"
if [ ! -x "$WYN" ]; then
    echo "SKIP: no wyn binary (set WYN or WYN_ROOT)"
    exit 0
fi

PASS=0
FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# A stale src/ui.wyn.out silently ignores every edit to ui.wyn or an imported
# module - it is what made five mutation tests falsely survive while this feature
# was being built. Delete it before every run, not once.
run_script() {
    rm -f src/ui.wyn.out src/ui.wyn.c
    SDL_VIDEODRIVER=dummy WYNCANVAS_FRAMES=3 WYNCANVAS_SCRIPT="$1" \
        "$WYN" run src/ui.wyn 2>&1 | grep -E '^  (screen|selat|marquee)'
}

echo "=== Running marching-ants test ==="

# An 80x80 ellipse inscribed in doc (20,20)..(99,99).
#
# WHY x=30. At x=30 the ellipse's rim sits near y=33, which is 13 pixels below
# the bounding box's top edge (y=20) and 10 to the right of its left edge - so a
# bright pixel there cannot be a box line. That is the whole discrimination this
# test is built on.
SEL='tool:mellipse,marquee:20:20:100:100'

# First: the selection really is an ellipse and not a rect. Area pi*40^2 ~ 5027,
# and the bbox CORNER must be unselected - a rect would report 1.000 there.
OUT=$(run_script "$SEL,selat:60:60,selat:22:22,selat:30:33")
if printf '%s' "$OUT" | grep -q 'selat(60,60)=1.000'; then
  ok "the ellipse's centre is selected"
else
  bad "centre not selected"; printf '%s\n' "$OUT" | sed 's/^/        /'
fi
if printf '%s' "$OUT" | grep -q 'selat(22,22)=0.000'; then
  ok "the bbox CORNER is not selected (so it is an ellipse, not a rect)"
else
  bad "corner was selected - selection is not an ellipse"; printf '%s\n' "$OUT" | sed 's/^/        /'
fi
# The rim pixel is partially covered: between 0 and 1, which is also what makes
# the outline there a gradient rather than a hard step.
if printf '%s' "$OUT" | grep -qE 'selat\(30,33\)=0\.[0-9]+'; then
  ok "the rim pixel at (30,33) is partially covered"
else
  bad "rim pixel not partial"; printf '%s\n' "$OUT" | sed 's/^/        /'
fi

# Now the pixels. The rim must be BRIGHT (the ants) and the interior must not be.
#
# Thresholded rather than matched exactly: the document underneath is a generated
# scene, so the interior colour is not a fixed constant, and the ants are drawn
# over whatever is there. "Rim is much brighter than interior" is the property
# that distinguishes a drawn outline from an absent one, and it holds regardless
# of the image.
OUT=$(run_script "$SEL,screen:30:33,screen:30:50")
RIM=$(printf '%s' "$OUT" | sed -n 's/.*doc(30,33).*rgb=\([0-9]*\),.*/\1/p')
INNER=$(printf '%s' "$OUT" | sed -n 's/.*doc(30,50).*rgb=\([0-9]*\),.*/\1/p')
if [ -n "$RIM" ] && [ -n "$INNER" ]; then
  ok "both probes returned a pixel (rim=$RIM inner=$INNER)"
  if [ "$RIM" -gt 200 ]; then
    ok "the ellipse rim is bright - the ants are drawn there (r=$RIM)"
  else
    bad "rim is not bright (r=$RIM) - the outline did not reach the screen"
  fi
  if [ "$RIM" -gt "$INNER" ]; then
    ok "the rim is brighter than the interior (${RIM} > ${INNER})"
  else
    bad "rim ($RIM) is not brighter than interior ($INNER)"
  fi
else
  bad "a probe returned no pixel"; printf '%s\n' "$OUT" | sed 's/^/        /'
fi

# Deselecting must REMOVE the ants, not leave them behind. This is what the
# generation stamp buys: the overlay is rebuilt when the selection changes, so a
# cleared selection clears the outline.
OUT=$(run_script "$SEL,sel:none,screen:30:33")
GONE=$(printf '%s' "$OUT" | sed -n 's/.*doc(30,33).*rgb=\([0-9]*\),.*/\1/p')
if [ -n "$GONE" ] && [ "$GONE" -lt 200 ]; then
  ok "deselecting removes the ants (r=$GONE)"
else
  bad "ants survived a deselect (r=${GONE:-none})"; printf '%s\n' "$OUT" | sed 's/^/        /'
fi

# A rect selection's ants follow the rect, which is the case where ants and bbox
# agree - included so a regression that drew ONLY the bbox cannot pass the whole
# file by accident.
OUT=$(run_script "tool:mrect,marquee:30:30:70:70,screen:30:50,screen:50:50")
EDGE=$(printf '%s' "$OUT" | sed -n 's/.*doc(30,50).*rgb=\([0-9]*\),.*/\1/p')
MID=$(printf '%s' "$OUT" | sed -n 's/.*doc(50,50).*rgb=\([0-9]*\),.*/\1/p')
if [ -n "$EDGE" ] && [ -n "$MID" ] && [ "$EDGE" -gt "$MID" ]; then
  ok "a rect selection is outlined on its edge, not through its middle"
else
  bad "rect edge=${EDGE:-none} mid=${MID:-none}"; printf '%s\n' "$OUT" | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "ants: $PASS pass, 0 fail"
  exit 0
fi
echo "ants: $PASS pass, $FAIL fail"
exit 1
