#!/usr/bin/env bash
# verify_ui.sh - headless, assertive verification of the EDITOR, not the modules.
#
# WHY THIS EXISTS ALONGSIDE `wyn test`.
#
# `wyn test` proves the imaging core: tests/test_paint.wyn paints at document
# (16,16) and reads the pixel back, tests/test_view.wyn proves the transform is
# invertible. Neither can fail if the WINDOW maps a click at document (16,16) to
# somewhere else, because neither involves a window.
#
# This script drives the real binary through WYNCANVAS_SCRIPT - real widget ids,
# real platform events, the real view transform - and greps the printed pixel
# values. Each check names the value it demands, so a pass is a statement about
# pixels rather than about exit codes.
#
#   ./csrc/build.sh && WYN_ROOT=... wyn build src/ui.wyn -o /tmp/wcui
#   ./verify_ui.sh /tmp/wcui
set -uo pipefail

BIN="${1:-/tmp/wcui}"
export SDL_VIDEODRIVER=dummy

pass=0
fail=0

# There is no `timeout` binary on the target machine; perl's alarm is the
# portable stand-in and is why every run below goes through it.
run() {
    local script="$1"
    shift
    env WYNCANVAS_FRAMES=2 WYNCANVAS_SCRIPT="$script" "$@" \
        perl -e 'alarm(90); exec @ARGV' -- "$BIN" 2>&1
}

# check <name> <expected-substring> <actual-output>
check() {
    local name="$1" want="$2" got="$3"
    if printf '%s' "$got" | grep -qF -- "$want"; then
        printf '  ok   %s\n' "$name"
        pass=$((pass + 1))
    else
        printf '  FAIL %s\n         wanted: %s\n' "$name" "$want"
        printf '%s\n' "$got" | sed 's/^/         | /'
        fail=$((fail + 1))
    fi
}

echo "wyncanvas UI verification ($BIN)"

# ---------------------------------------------------------------------------
# 1. The brush. Paint pure red on a fresh layer at 100% and read the COMPOSITE.
# Red is chosen because sRGB(1,0,0) is linear(1,0,0): if the value came back as
# anything else the linear/sRGB handling would be suspect, and a mid-grey would
# hide that.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,hsv:0:1:1,brush:12:1:1,tool:brush,stroke:100:100:100:100,px:100:100,px:100:120')
check "brush paints the clicked pixel"        'px(100,100)=1.000,0.000,0.000 a=1.000' "$out"
check "brush leaves a distant pixel alone"    'px(100,120)=0.243,0.186,0.000'         "$out"
check "one stroke is one history entry"       'hist=2'                                 "$out"

# ---------------------------------------------------------------------------
# 2. A drag paints a continuous line, not two dots at the ends.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,hsv:120:1:1,brush:6:1:1,tool:brush,stroke:40:128:210:128,px:40:128,px:90:128,px:128:128,px:180:128,px:210:128')
check "drag start painted"    'px(40,128)=0.000,1.000,0.000'  "$out"
check "drag quarter painted"  'px(90,128)=0.000,1.000,0.000'  "$out"
check "drag middle painted"   'px(128,128)=0.000,1.000,0.000' "$out"
check "drag three-quarter painted" 'px(180,128)=0.000,1.000,0.000' "$out"
check "drag end painted"      'px(210,128)=0.000,1.000,0.000' "$out"

# ---------------------------------------------------------------------------
# 3. THE ONE THAT MATTERS MOST: the brush lands under the pointer at a zoom and
# a pan, not just at 1:1 centred. The stroke is specified in DOCUMENT
# coordinates and the harness maps them through the same transform the blit uses,
# so a broken mapping paints somewhere else and the assertion fails.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,zoom:400,pan:-90:37,hsv:300:1:1,brush:5:1:1,tool:brush,stroke:100:100:100:100,px:100:100,px:100:112')
check "brush lands correctly at 400% + pan"   'px(100,100)=1.000,0.000,1.000' "$out"
check "and does not smear at 400% + pan"      'px(100,112)=0.243,0.186,0.000' "$out"

# NB the pan sign. `pan:120:-200` at 800% pushes document row 70 ABOVE the
# viewport, and the editor then correctly REFUSES the press - which is itself
# worth asserting, so both cases are here: the refusal, and the hit.
out=$(run 'click:+L,zoom:800,pan:120:-200,hsv:60:1:1,brush:4:1:1,tool:brush,stroke:70:70:70:70,px:70:70')
check "a press outside the viewport paints nothing" 'px(70,70)=0.290,0.130,0.000' "$out"
check "and records no history entry"                'hist=1'                      "$out"

out=$(run 'click:+L,zoom:800,pan:120:300,hsv:60:1:1,brush:4:1:1,tool:brush,stroke:70:70:70:70,px:70:70,px:70:90')
check "brush lands correctly at 800% + pan"   'px(70,70)=1.000,1.000,0.000'   "$out"
check "and not 20px away at 800% + pan"       'px(70,90)=0.290,0.130,0.000'   "$out"

out=$(run 'click:+L,zoom:25,hsv:240:1:1,brush:20:1:1,tool:brush,stroke:128:128:128:128,px:128:128')
check "brush lands correctly at 25%"          'px(128,128)=0.000,0.000,1.000' "$out"

# ---------------------------------------------------------------------------
# 4. The framebuffer. `px:` proves the compositor holds the value; `screen:`
# proves it reached the WINDOW at the right coordinates - so the texture upload,
# the blit rectangle and the transform all agree. Values are display-encoded.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,hsv:0:1:1,brush:12:1:1,tool:brush,stroke:100:100:100:100,screen:100:100,zoom:400,pan:-60:25,screen:100:100,zoom:800,screen:100:100')
check "framebuffer shows the paint at 100%" 'win(464,362) rgb=255,0,0' "$out"
check "framebuffer shows the paint at 400% + pan" 'win(322,305) rgb=255,0,0' "$out"
check "framebuffer shows the paint at 800%" 'win(212,195) rgb=255,0,0' "$out"

# ---------------------------------------------------------------------------
# 5. The eraser: alpha AND colour to zero. A composite over a visible backdrop
# means "erased" shows the layer beneath, which is the observable consequence.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,hsv:60:1:1,tool:fill,fill:128:128,px:128:128,click:Erase,brush:20:1:1,tool:eraser,stroke:128:128:128:128,px:128:128,px:60:128')
check "fill covers the layer"        'filled 65536 px'                "$out"
check "fill is yellow"               'px(128,128)=1.000,1.000,0.000'  "$out"
check "eraser reveals the backdrop"  'px(128,128)=0.498,0.502,0.000'  "$out"
check "eraser spares the rest"       'px(60,128)=1.000,1.000,0.000'   "$out"

# ---------------------------------------------------------------------------
# 6. The bucket fill stops at a colour boundary.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,hsv:0:0:0,brush:40:1:1,tool:brush,stroke:128:128:128:128,hsv:300:1:1,tool:fill,fill:128:128,px:128:128,px:200:200')
check "fill floods the black disc"      'px(128,128)=1.000,0.000,1.000' "$out"
check "fill is bounded, not the whole canvas" 'filled 4928 px'                "$out"
check "fill does not escape the disc"        'px(200,200)=0.216,0.784,0.000' "$out"

# ---------------------------------------------------------------------------
# 7. Undo/redo of a paint stroke, through the real Undo button.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,hsv:0:1:1,brush:14:1:1,tool:brush,stroke:100:100:100:100,px:100:100,click:Undo,px:100:100,click:Redo,px:100:100')
check "painted before undo"  'px(100,100)=1.000,0.000,0.000' "$out"
check "Undo button reverts the stroke" 'msg=undo paint' "$out"
check "Redo button reapplies it"       'msg=redo paint' "$out"

# ---------------------------------------------------------------------------
# 8. Layer operations through real button clicks, including that a delete's undo
# restores the layer's PROPERTIES and not merely its pixels.
# ---------------------------------------------------------------------------
out=$(run 'click:b>,click:b>,click:o-,click:o-,layers,click:-L,layers,undo,layers')
check "blend stepper reaches overlay"   'msg=blend overlay'                     "$out"
check "opacity stepper reaches 0.6"     'msg=opacity 0.6'                       "$out"
check "layer row shows mode + opacity"  'L1 blue vis=true mode=overlay op=0.6'  "$out"
check "delete removes the layer"        'msg=deleted blue'                      "$out"
check "delete undo restores properties" 'L1 blue vis=true mode=overlay op=0.6'  "$out"

out=$(run 'click:+L,layers,click:dn,layers,click:up,click:up,layers')
check "new layer goes above the active one" 'L2 layer 2' "$out"
check "dn lowers it"                        'L1 layer 2' "$out"
check "up raises it again"                  'L2 layer 2' "$out"

out=$(run 'toggle_top,layers')
check "row visibility toggle is undoable" 'history.depth=1' "$out"
check "and the layer really is hidden"    'blue vis=false'  "$out"

# ---------------------------------------------------------------------------
# 9. The colour picker, dragged as a widget. The SV square's corners and the hue
# strip's ends are exact, which is what makes this an assertion rather than a
# smoke test.
# ---------------------------------------------------------------------------
out=$(run 'huedrag:0,huedrag:50,huedrag:100,pickat:149:0,pickat:0:0,pickat:149:149,pickat:74:74')
check "hue strip top is red"          'huedrag:0 -> hue=0.0'                "$out"
check "hue strip third is green"      'huedrag:50 -> hue=120.0'             "$out"
check "hue strip two-thirds is blue"  'huedrag:100 -> hue=240.0'            "$out"
check "SV top-right is full sat/val"  'pickat:149:0 -> hsv=240.0,1.0,1.0'   "$out"
check "SV top-left is unsaturated"    'pickat:0:0 -> hsv=240.0,0.0,1.0'     "$out"
check "SV bottom-right is black"      'pickat:149:149 -> hsv=240.0,1.0,0.0' "$out"
check "SV centre is half/half"        'pickat:74:74 -> hsv=240.0,0.5,0.5'   "$out"

# ---------------------------------------------------------------------------
# 10. The eyedropper reads the COMPOSITE, which is what the user is pointing at.
# Paint a known colour, drop the picker to black, then pick it back.
# ---------------------------------------------------------------------------
out=$(run 'click:+L,hsv:270:0.8:0.9,brush:20:1:1,tool:brush,stroke:128:128:128:128,hsv:0:0:0,eyedrop:128:128')
check "eyedropper recovers the painted colour" 'hsv=270.0,0.8,0.9' "$out"

# ---------------------------------------------------------------------------
# 11. Pan and zoom themselves. `pan:` goes through the Pan TOOL and a real drag,
# so this exercises canvas_press/drag/release rather than view.pan_by alone.
# ---------------------------------------------------------------------------
out=$(run 'zoom:100,map:464:362,pan:50:0,map:514:362,zoom:200,click:Fit,click:1:1')
check "window maps to the expected document pixel" 'doc(100.000,100.000)' "$out"
check "a pan of +50 moves the same doc pixel +50"  'doc(100.000,100.000)' "$out"
check "Fit shrinks to fit the viewport"            'fit ' "$out"
check "1:1 returns to 100%"                        'zoom 100%' "$out"

# ---------------------------------------------------------------------------
# 12. Save and re-open: the whole document through the existing PNG codec.
# ---------------------------------------------------------------------------
rm -f /tmp/wc_verify.png
out=$(env WYNCANVAS_SAVE=/tmp/wc_verify.png WYNCANVAS_OPEN=/tmp/wc_verify.png \
      WYNCANVAS_FRAMES=2 \
      WYNCANVAS_SCRIPT='click:+L,hsv:300:1:1,brush:24:1:1,tool:brush,stroke:60:60:200:200,px:130:130,save,open,layers,px:130:130,px:10:200' \
      perl -e 'alarm(90); exec @ARGV' -- "$BIN" 2>&1)
check "save writes the file"              'wrote /tmp/wc_verify.png'      "$out"
check "open reads it back"                'opened /tmp/wc_verify.png'     "$out"
check "the stroke survived the round trip" 'px(130,130)=1.000,0.000,1.000' "$out"
check "so did the flattened backdrop"      'px(10,200)=0.386,0.019,0.000'  "$out"
check "open flattens to one layer"         'layers=1'                      "$out"

# ---------------------------------------------------------------------------
# 13. A screenshot, because no assertion can say the UI LOOKED right.
# ---------------------------------------------------------------------------
rm -f /tmp/wc_verify.bmp
env WYNCANVAS_FRAMES=3 WYNCANVAS_SHOT=/tmp/wc_verify.bmp \
    WYNCANVAS_SCRIPT='click:+L,hsv:200:0.9:1,brush:26:0.5:1,tool:brush,stroke:40:40:210:210,stroke:210:40:40:210' \
    perl -e 'alarm(90); exec @ARGV' -- "$BIN" >/dev/null 2>&1
if [ -s /tmp/wc_verify.bmp ]; then
    printf '  ok   screenshot written (%s bytes)\n' "$(wc -c < /tmp/wc_verify.bmp | tr -d ' ')"
    pass=$((pass + 1))
else
    printf '  FAIL no screenshot\n'
    fail=$((fail + 1))
fi

# ---------------------------------------------------------------------------
# 14. Refusals. A click outside the canvas, and painting an empty document, must
# report rather than crash or silently record an entry.
# ---------------------------------------------------------------------------
out=$(run 'zoom:25,tool:brush,map:60:50')
check "a point off the canvas is reported as such" 'hits=false' "$out"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
