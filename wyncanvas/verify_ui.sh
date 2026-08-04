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
# 4936, not the old 4928: the improved brush falloff (smoothstep, not a hard
# ramp) puts a few more antialiased-rim pixels of the black disc above the
# bucket's colour-match threshold, so the flood reaches 8 more of them. The point
# of the check is that the fill is BOUNDED (a few thousand px, not the 65536 of
# the whole canvas), which still holds; the exact count tracks the brush edge.
check "fill is bounded, not the whole canvas" 'filled 4936 px'                "$out"
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
# 15. SELECTIONS. A marquee dragged in DOCUMENT coordinates through the window
# path, exactly as `stroke:` is - so these assert the marquee's MAPPING and not
# select.rect's arithmetic, which tests/test_select.wyn already covers.
#
# `active:0` is the project's standing trap and is not optional: layer 1 is
# `multiply` at 80%, so anything painted there barely moves the composite.
#
# The area assertion is the one that cannot pass by accident. A marquee applied at
# every drag move WITHOUT restoring the pre-drag state first would accumulate all
# nine intermediate rectangles and report their union - a bigger number than 6400.
# ---------------------------------------------------------------------------
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,selat:80:80,selat:200:200,selat:39:80')
check "a rect marquee selects its exact area"  'area=6400.0'          "$out"
check "and its bounding box is the dragged box" 'bbox=40,40,119,119'   "$out"
check "a pixel inside reads full coverage"      'selat(80,80)=1.000'   "$out"
check "a pixel outside reads none"              'selat(200,200)=0.000' "$out"
check "and the pixel one short of the edge too" 'selat(39,80)=0.000'   "$out"

# THE MARQUEE AT A ZOOM AND A PAN. This is the selection's equivalent of check 3,
# and it is the reason the anchor is stored in document coordinates: at 400% with
# a pan the window positions are completely different, and the selection must
# still land on the named document pixels.
out=$(run 'active:0,zoom:400,pan:-90:37,tool:mrect,marquee:95:95:105:105,selat:100:100,selat:90:90')
check "a marquee maps correctly at 400% + pan" 'selat(100,100)=1.000' "$out"
check "and does not select outside it there"   'selat(90,90)=0.000'   "$out"

# An ellipse is inscribed in the dragged box, so its CORNER is unselected while
# its centre is not - which is what distinguishes it from the rect tool. An
# ellipse of radius 40 has area pi*r^2 = 5026, not 6400.
out=$(run 'active:0,tool:mellipse,marquee:40:40:120:120,selat:80:80,selat:42:42')
check "an ellipse selects its centre"        'selat(80,80)=1.000' "$out"
check "and not the corner of its box"        'selat(42,42)=0.000' "$out"
check "so its area is pi r^2, not the box's" 'area=5028.3'        "$out"

# Boolean ops, through two drags. `subtract` is the interesting one: it must leave
# the first rect MINUS the overlap, i.e. an area strictly between 0 and the first.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,selop:add,marquee:120:40:200:120,selat:160:80')
check "add extends the selection"       'area=12800.0'         "$out"
check "into the added region"           'selat(160,80)=1.000'  "$out"

out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,selop:subtract,marquee:80:40:200:120,selat:60:80,selat:100:80')
check "subtract removes the overlap"    'area=3200.0'          "$out"
check "keeping the part not overlapped" 'selat(60,80)=1.000'   "$out"
check "and dropping the part that was"  'selat(100,80)=0.000'  "$out"

out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,selop:intersect,marquee:80:40:200:120,selat:100:80,selat:60:80')
check "intersect keeps only the overlap" 'area=3200.0'         "$out"
check "which is the shared region"       'selat(100,80)=1.000' "$out"
check "and not the rest of the first"    'selat(60,80)=0.000'  "$out"

# Select all / none / invert. Invert of a rect on a 256x256 document is
# 65536-6400 = 59136, which is an exact number and therefore an assertion.
out=$(run 'active:0,sel:all,sel:invert,sel:all,sel:none')
check "select all covers the document" 'area=65536.0' "$out"
check "and none empties it"            'empty=true'   "$out"

out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,sel:invert,selat:80:80,selat:200:200')
check "invert of a rect is its complement" 'area=59136.0'         "$out"
check "so the old inside is now outside"   'selat(80,80)=0.000'   "$out"
check "and the old outside is now inside"  'selat(200,200)=1.000' "$out"

# A click with no drag DESELECTS. Without this a stray click leaves a 0x0
# selection and every pixel tool silently refuses everywhere.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,marquee:80:80:80:80,sel:all')
check "a zero-area marquee deselects" 'deselected' "$out"

# FEATHER, which is the reason coverage is a float and not a bit. A pixel just
# inside a feathered edge must read a FRACTION - a boolean selection cannot
# express it, and `selat:` printing three decimals is what makes it assertable.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,sel:feather:6,selat:80:80,selat:40:80,selat:120:80')
check "feather leaves the interior fully selected" 'selat(80,80)=1.000' "$out"
check "and the edge PARTIALLY selected"            'selat(40,80)=0.529' "$out"
check "fading outside the original boundary"       'selat(120,80)=0.471' "$out"

# ---------------------------------------------------------------------------
# 16. THE SELECTION PROTECTS THE REST OF THE LAYER. This is the check the whole
# selection feature exists for: a brush aimed OUTSIDE it must change NOTHING, and
# must record no history entry either - a refused stroke that still pushed an
# entry would make Undo appear to do nothing.
# ---------------------------------------------------------------------------
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,hsv:0:1:1,brush:12:1:1,tool:brush,stroke:200:200:200:200,px:200:200,stroke:80:80:80:80,px:80:80')
check "a brush stroke OUTSIDE a selection is refused" 'outside the selection' "$out"
check "and changes nothing"                   'px(200,200)=0.216,0.784,0.000' "$out"
check "and records no history entry"           'stroke:200:200:200:200 -> outside the selection hist=0' "$out"
check "while a stroke INSIDE it still paints"  'px(80,80)=0.400,0.000,0.000'   "$out"

# ---------------------------------------------------------------------------
# 17. FILTERS. Each is ONE undo entry that restores the EXACT prior pixels, and
# each honours the selection.
#
# INVERT IS THE FIRST FILTER CHECKED BECAUSE IT IS EXACTLY REVERSIBLE, so
# "px before == px after undo" tests the history record rather than the filter's
# arithmetic. Blur is checked separately BECAUSE it is not reversible: for blur,
# undo restoring the prior value is a statement about the snapshot and nothing
# else, which is the point.
# ---------------------------------------------------------------------------
# THE UNTOUCHED VALUE IS NOT PRINTED IN THIS RUN, and that is deliberate: `check`
# greps the WHOLE output, so a script that printed the pre-filter pixel and then
# the post-undo one would satisfy "undo restored 0.275" from the FIRST print even
# if undo did nothing at all. Measured - a mutation that snapshotted the "before"
# AFTER the filter (making undo a no-op) passed the earlier version of this check.
# So the run below prints the pixel ONLY after the filter and after the undo: the
# two values differ, so each grep can only be satisfied by its own line.
out=$(run 'active:0,filter:invert,px:80:80,undo,px:80:80')
check "invert changes the pixel"          'px(80,80)=0.125,0.326,0.925' "$out"
check "as one undo entry, named"          'undo=invert'                 "$out"
check "undo restores the EXACT prior value" 'px(80,80)=0.275,0.149,0.000' "$out"
check "and the status names what it undid" 'msg=undo invert'             "$out"

out=$(run 'active:0,filter:invert,undo,redo,px:80:80')
check "redo reapplies it"                  'msg=redo invert'             "$out"
check "restoring the filtered value"       'px(80,80)=0.125,0.326,0.925' "$out"

# A BLUR INSIDE A SELECTION LEAVES THE OUTSIDE BIT-IDENTICAL.
#
# THE GRADIENT IS BUILT WITH A FILTER, NOT WITH THE BRUSH, and that is deliberate.
# A blur only changes a pixel where there is a gradient, so this check needs a hard
# edge next to the selection boundary - and using a brush dab for it would couple
# these assertions to the brush KERNEL, whose exact soft edge is not what is under
# test here. `invert` inside a rect selection produces a perfectly hard step at
# x=120 using only the two features this block is about.
#
# The probes are at x=117 and x=119: INSIDE the inverted patch, in the blur's reach
# of the step at 120, and OUTSIDE the 122..200 selection that the blur is given. So
# a blur that ignored its mask would change them - which the control block below
# measures, printing the moved values.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,filter:invert,sel:none,tool:mrect,marquee:122:40:200:200,filter:blur,px:117:80,px:119:80')
check "a blur inside a selection leaves x=117 bit-identical" 'px(117,80)=0.184,0.257,0.925' "$out"
check "and x=119 bit-identical too"          'px(119,80)=0.187,0.253,0.925' "$out"
check "while the blur really did run"        'blur on selection'            "$out"

# The control: the SAME script with NO selection for the blur. Both pixels move,
# which is what proves the mask above was doing the work rather than the blur
# being a no-op in a flat region.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,filter:invert,sel:none,filter:blur,px:117:80,px:119:80')
check "with NO selection the same blur moves x=117" 'px(117,80)=0.184,0.256,0.900' "$out"
check "and moves x=119 much further"                'px(119,80)=0.194,0.244,0.601' "$out"
check "and says so"                                 'blur on layer'                "$out"

# Undo after a BLUR - the irreversible filter - must restore the exact prior
# value from the snapshot. 0.148,0.135 is the blurred value and 0.227,0.205 the
# original, so this check names both and cannot pass if the record were taken
# after the filter instead of before.
# UNDO AFTER A BLUR - the irreversible filter - must restore the exact prior value
# from the snapshot. The gradient is again made by inverting a rect rather than by
# a brush dab, for the reason given above. Again the pre-blur pixel is NOT printed
# before the blur, so 0.187,0.253 can only reach this output by being RESTORED.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:120,filter:invert,sel:none,filter:blur,px:119:80,undo,px:119:80')
check "blur changes the pixel"                'px(119,80)=0.194,0.244,0.601' "$out"
check "undo after a BLUR restores it exactly" 'px(119,80)=0.187,0.253,0.925' "$out"
check "and names the operation"               'msg=undo blur'                "$out"

# The remaining filters, each one entry with its own label, so a filter wired to
# the wrong handler or recording nothing is caught by name.
out=$(run 'active:0,filter:sharpen,filter:saturate,filter:desaturate,filter:levels,filter:curve')
check "sharpen records one entry"    'filter:sharpen -> sharpen on layer hist=1'       "$out"
check "saturate records one entry"   'filter:saturate -> saturate on layer hist=2'     "$out"
check "desaturate records one entry" 'filter:desaturate -> desaturate on layer hist=3' "$out"
check "levels records one entry"     'filter:levels -> levels on layer hist=4'         "$out"
check "curve records one entry"      'filter:curve -> curve on layer hist=5'           "$out"

# A filter with an EMPTY selection must filter the LAYER, not nothing. The empty
# selection buffer is all zeros, so passing its handle rather than 0 would make
# every filter a silent no-op - the single most likely way to get this wrong.
out=$(run 'active:0,sel:none,px:80:80,filter:invert,px:80:80')
check "a filter with nothing selected filters the layer" 'invert on layer'             "$out"
check "and really does change the pixel"                 'px(80,80)=0.125,0.326,0.925' "$out"

# ---------------------------------------------------------------------------
# 18. THE TEXT TOOL. Click to place, one undo entry, in the brush's colour.
#
# `text:x:y:string` goes through the tool's own path - it selects the tool,
# presses the canvas at the WINDOW position document (x,y) maps to, and services
# the resulting request - so a wrong mapping places the text somewhere else and
# the pixel assertion fails. Calling commit_text directly could not fail that way.
#
# The colour is the assertion that catches the most likely bug. paint.color_linear
# is what the brush passes to the SAME kernel; passing the sRGB value instead
# would paint a visibly lighter glyph. Pure red is chosen because sRGB(1,0,0) and
# linear(1,0,0) are equal ONLY for red's own channel - the composite over the blue
# multiply layer at 80% gives 0.400,0.000,0.000, and a glyph painted in sRGB
# rather than linear would not land on that number.
#
# NB: these run on a machine with a usable system font. `text.default_font()`
# returns 0 where there is none, which the tool reports as "no system font
# available" - a fact about the machine, not a failure, and the reason this block
# names the font path it used.
# ---------------------------------------------------------------------------
out=$(run 'active:0,hsv:0:1:1,textpx:60,px:60:60,text:40:40:WYN,px:60:60,px:200:200')
check "text ink lands on the placed glyph"   'px(60,60)=0.400,0.000,0.000'   "$out"
check "in the brush's LINEAR colour"         'text "WYN" at 40,40'           "$out"
check "as ONE history entry"                 'hist=1'                        "$out"
check "labelled 'text', not 'paint'"         'undo=text'                     "$out"
check "and leaves a distant pixel alone"     'px(200,200)=0.216,0.784,0.000' "$out"

# UNDO AFTER A TEXT COMMIT RESTORES THE EXACT PRIOR PIXELS. Like a blur, a
# coverage-weighted glyph blend has no inverse to store, so this is a statement
# about the snapshot: 0.306,0.112 is the untouched ramp at (60,60) and 0.400,0.000
# is the inked value, so the check cannot pass if the "before" had been taken
# after the draw.
# The untouched ramp value is NOT printed before the commit here - see the note
# above the invert block for the measured reason.
out=$(run 'active:0,hsv:0:1:1,textpx:60,text:40:40:WYN,px:60:60,undo,px:60:60')
check "text changes the pixel"                 'px(60,60)=0.400,0.000,0.000' "$out"
check "undo after TEXT restores it exactly"    'px(60,60)=0.306,0.112,0.000' "$out"
check "and the status names it"                'msg=undo text'               "$out"

out=$(run 'active:0,hsv:0:1:1,textpx:60,text:40:40:WYN,undo,redo,px:60:60')
check "redo re-inks the same pixel"            'msg=redo text'               "$out"
check "back to the inked value"                'px(60,60)=0.400,0.000,0.000' "$out"

# An empty string must be REFUSED and record nothing: a history entry whose two
# snapshots are identical would make Undo appear broken.
out=$(run 'active:0,text:40:40:')
check "empty text is refused"        'type some text first' "$out"
check "and records no undo entry"    'hist=0'               "$out"

# ---------------------------------------------------------------------------
# 12b. THE LAYERED DOCUMENT (.wync), which is the one PNG cannot do.
#
# Check 12 above proves the PNG path, and a PNG round trip FLATTENS - it comes
# back as one layer, which is why that check asserts `layers=1`. So a .wync round
# trip that merely "worked" would be indistinguishable from the PNG one unless
# the assertions name the things flattening destroys. These four do:
#
#   the layer COUNT (2, not 1),
#   a per-layer BLEND MODE (multiply, which a flatten bakes in and forgets),
#   a per-layer OPACITY (0.8, likewise),
#   and a PIXEL, so the stack is not merely structurally right but numerically so.
#
# `active:0` before the stroke is deliberate and is the project's standing trap:
# layer 1 is `multiply` at 80%, so magenta painted THERE barely changes the
# composite and a broken brush would look the same as a working one. Layer 0 is
# `normal` at 100%.
#
# The expected pixel is 0.400,0.000,0.925 rather than pure magenta BECAUSE the
# blue multiply layer is still above it - which is itself the point: if the round
# trip had dropped the blend mode or the opacity, this pixel would come back as
# something else, so one number covers both.
# ---------------------------------------------------------------------------
rm -f /tmp/wc_verify.wync
out=$(run 'active:0,hsv:300:1:1,brush:20:1:1,tool:brush,stroke:100:100:100:100,px:100:100,saveas:/tmp/wc_verify.wync,openas:/tmp/wc_verify.wync,layers,px:100:100')
check "wync save reports the layer count"    'wrote /tmp/wc_verify.wync (2 layers)' "$out"
check "wync open reports the layer count"    'opened /tmp/wc_verify.wync (2 layers)' "$out"
check "wync keeps the STACK, not a flatten"  'layers=2'                             "$out"
check "wync preserves a blend mode"          'L1 blue vis=true mode=multiply'       "$out"
check "wync preserves an opacity"            'op=0.8'                               "$out"
check "wync preserves a painted pixel"       'px(100,100)=0.400,0.000,0.925'        "$out"

# A .png path must still FLATTEN, from the same Save. The suffix picks the format,
# so this is the assertion that the dispatch is a dispatch and not a rename.
rm -f /tmp/wc_verify2.png
out=$(run 'active:0,saveas:/tmp/wc_verify2.png,openas:/tmp/wc_verify2.png,layers')
check "a .png path still writes a PNG"   'wrote /tmp/wc_verify2.png' "$out"
check "and a PNG open still flattens"    'layers=1'                  "$out"

# A damaged .wync must REPORT and leave the open document alone. The fixture is
# built by truncating a good file, which is the only way to make one from a shell:
# a hand-written "bad file" would test the magic check and nothing else.
head -c 40 /tmp/wc_verify.wync > /tmp/wc_verify_trunc.wync
out=$(run 'openas:/tmp/wc_verify_trunc.wync,layers')
check "a truncated wync is reported, not guessed" 'cannot open /tmp/wc_verify_trunc.wync' "$out"
check "and the open document survives it"         'layers=2'                              "$out"

# ---------------------------------------------------------------------------
# 15. WHOLE-DOCUMENT TRANSFORMS: rotate 90 CW/CCW, crop to selection, scale.
#
# These were unreachable from the editor until OP_DOC_XFORM existed, because the
# OP_PIXELS undo restores with wynimg_copy_into (which requires the size to be
# unchanged) and a texture cannot be resized under its handle.
#
# EVERY CHECK READS ALL FOUR SIZES, which is the point of the `doc` action:
# the document's, the SELECTION's, the active layer's BUFFER, and what the VIEW is
# fitting. g_doc_w alone would assert the easiest quarter. A selection left at the
# old size makes every later clip read out of bounds; a layer buffer left at the old
# size makes wynimg_composite refuse that layer silently; a stale view puts the
# pointer in the wrong place. All three are invisible in a screenshot, which is why
# they are asserted here and not left to section 13.
#
# The TEXTURE is proved separately, by section 2's `winpx` checks: it is created by
# refresh() inside the frame loop, so a script (which runs before the first frame)
# legitimately sees 0x0 and asserting it would test the harness.
# ---------------------------------------------------------------------------

# Rotate 90 CW then CCW on a NON-SQUARE document, so the w/h swap is observable -
# on the default 256x256 canvas a rotation that did nothing at all would pass.
# The crop establishes the non-square document, so it is checked on the way.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:100,doc,key:y,doc,key:n,doc,key:p,doc')
check "crop to the marquee resizes the document"   'msg=crop -> 80x60'                "$out"
check "crop moves doc, selection, buffer and view"      'doc=80x60 sel=80x60 buf=80x60 vfit=80x60'    "$out"
check "rotate 90 CW swaps w and h"                 'doc=60x80 sel=60x80 buf=60x80 vfit=60x80'    "$out"
check "rotate 90 CCW swaps them back"              'doc=80x60 sel=80x60 buf=80x60 vfit=80x60'    "$out"
check "and each is one undo entry, named"          'hist=3 undo=rotate 90 CCW'        "$out"

# The crop rect is the selection's bbox, which is INCLUSIVE at both ends - so a
# marquee from 40,40 to 120,100 is 80x60, not 81x61 or 79x59. A one-pixel error here
# looks like a rounding artefact rather than a bug, which is why it gets its own check.
out=$(run 'active:0,tool:mrect,marquee:10:20:30:50,key:y,doc')
check "the crop rect is the bbox, inclusive"       'doc=20x30 sel=20x30 buf=20x30 vfit=20x30'    "$out"

# Crop with NO selection must refuse and change nothing: cropping to an empty
# selection has no meaningful answer, and a 0-sized document is a destroyed one.
out=$(run 'active:0,key:y,doc')
check "crop with no selection is refused"          'msg=select an area to crop to'    "$out"
check "and the document is untouched"              'doc=256x256'                      "$out"
check "and nothing was recorded"                   'hist=0'                           "$out"

# UNDO of a document transform must restore the SIZE as well as the pixels. This is
# the check that would fail if on_undo did not call sync_doc_size - the pixels would
# come back at the old size while the document still claimed the new one.
out=$(run 'active:0,tool:mrect,marquee:40:40:120:100,key:y,key:n,undo,doc,undo,doc,redo,doc')
check "undo of a rotate restores the crop's size"  'doc=80x60 sel=80x60 buf=80x60 vfit=80x60'    "$out"
check "undo of the crop restores the original"     'doc=256x256 sel=256x256 buf=256x256 vfit=256x256' "$out"
check "redo re-applies the crop's size"            'doc=80x60 sel=80x60 buf=80x60 vfit=80x60'    "$out"

# Scale, both directions, on the same key with shift for "larger". NOT on Cmd+- /
# Cmd+= (zoom), because zoom is non-destructive and a document scale resamples every
# layer - one modifier apart would be a footgun.
out=$(run 'key:u,doc,key:u+shift,doc,undo,doc,undo,doc')
check "scale halves the document"                  'msg=scale -> 128x128'             "$out"
check "and moves all four sizes"                  'doc=128x128 sel=128x128 buf=128x128 vfit=128x128' "$out"
check "shift+scale doubles it"                     'msg=scale -> 256x256'             "$out"
check "undo of a scale restores the halved size"   'doc=128x128 sel=128x128 buf=128x128 vfit=128x128' "$out"

# The DIMENSION-PRESERVING transforms must still work and must still be OP_PIXELS -
# i.e. they must NOT resize anything. A flip that went through the document path
# would be a needless full-document snapshot per flip.
out=$(run 'active:0,key:j,doc,key:r,doc')
check "flip-h does not resize the document"        'msg=flip-h on layer'              "$out"
check "and rotate180 does not either"              'doc=256x256 sel=256x256 buf=256x256'          "$out"

# ---------------------------------------------------------------------------
# 12b. THE PANEL'S STOREYS DO NOT PAINT OVER EACH OTHER.
#
# Found by looking at the screenshot section 13 writes, which nothing had ever
# done: the word "Layers" was ABSENT from the window. The panel's vertical stack
# was placed by three unrelated expressions over a 92px budget the three together
# needed 106px of, so the heading landed 16px inside the second row of layer
# buttons - and because paint() draws widgets LAST, the buttons covered it. All
# that survived was a stray "r" fragment between "<b" and "b>".
#
# WHY THIS IS A PIXEL CHECK AND NOT A LAYOUT ASSERTION. tests/test_layout.wyn now
# asserts the bands do not intersect, and that is the check that constrains the
# design. But it cannot see the PAINT ORDER: the heading and the buttons could be
# given disjoint bands and the heading still be invisible if something later drew
# over it. Only a window pixel settles that, which is exactly this script's job.
#
# (943,320) is the vertical stem of the "L" of "Layers" - a glyph interior, at the
# text colour 235,235,240 rather than an antialiased edge, so the value is exact
# and a one-pixel drift of the heading fails rather than dimming.
out=$(run 'winpx:943:320,winpx:1000:322')
check "the Layers heading is painted, not covered" 'winpx(943,320)=235,235,240' "$out"
# ...and the band to its right is bare panel, which is what proves the check above
# is reading a GLYPH and not a button face that happens to sit there. A button
# face is 52,52,62; panel background is 34,34,41.
check "the heading sits on bare panel, not on a button" 'winpx(1000,322)=34,34,41' "$out"

# ---------------------------------------------------------------------------
# 12c. THE THREE INTERACTION FACES, IN THE ORDER A FINGER IMPLIES.
#
# rest, hover and pressed must be TELLABLE APART, and pressed must RECEDE while
# hover ADVANCES - pressing pushes a control in. The old ramp did neither: rest
# 52,52,62, hover 70,110,170, pressed 50,90,150, which is hover BRIGHTER than
# pressed and the two only 1.34:1 apart. A pressed button looked like a hovered
# one that had dimmed a little.
#
# Note what made that survivable for so long: neither face was reachable from a
# script. `click:` presses and releases in one action, so it can only ever leave
# the resting face on screen, and nothing else moved the pointer at all. The
# `hover:` and `hold:` actions added alongside this section are what make the
# other two faces observable - the fix and the ability to see the fix arrived
# together, which is why they are one change.
#
# x=150,y=15 is the interior of the "Undo" button, clear of its glyphs, so these
# are face colours and not antialiased text.
out=$(run 'winpx:150:15,hover:Undo,winpx:150:15')
check "a button at rest is the neutral face"     'winpx(150,15)=52,52,62' "$out"
check "hovering a button ADVANCES it"            'winpx(150,15)=74,74,84' "$out"
out=$(run 'winpx:150:15,hold:Undo,winpx:150:15')
check "pressing a button RECEDES it"             'winpx(150,15)=28,28,34' "$out"
# Relative luminance of the three: .0119 < .0353 < .0699. Monotonic, which is the
# property the old ramp broke, and hover-vs-pressed is now 1.94:1 (was 1.34:1).

# THE SELECTED TOOL KEEPS LOOKING SELECTED WHILE THE POINTER IS ON IT.
#
# The sharpest of these, and a real defect in the old ramp rather than a
# refinement: hover and pressed were flat blues that REPLACED the green
# selected-tool colour, so hovering the selected Brush made it stop looking
# selected. The pointer destroyed the state the user was pointing at in order to
# check.
#
# Interaction is now a TRANSFORM of the base colour instead of a different
# colour, so both facts survive together. x=10,y=55 is the Brush button's
# interior; Brush is the tool selected at startup.
out=$(run 'winpx:10:55,hover:Brush,winpx:10:55')
check "the selected tool is green, not neutral"    'winpx(10,55)=32,100,82'  "$out"
check "hovering it stays GREEN, brighter"          'winpx(10,55)=54,122,104' "$out"
out=$(run 'winpx:10:55,hold:Brush,winpx:10:55')
check "pressing it stays green, darker"            'winpx(10,55)=17,55,45'   "$out"
# All three selected faces are green (g > r and g > b); under the old ramp two of
# the three were blue. Every one of the six faces above keeps the label at or
# above 4.5:1, WCAG AA for body text - the binding case is selected+hover, which
# is why the green is 32,100,82 and not a lighter, prettier one.

# A hovered button is not confusable with an UNhovered neighbour: "Redo" sits
# beside "Undo" and must stay at the resting face while Undo is hovered. Without
# this, a bug that lit every button on any hover would pass every check above.
out=$(run 'hover:Undo,winpx:150:15,winpx:215:15')
check "hover is per-widget, not global"          'winpx(215,15)=52,52,62' "$out"

# ---------------------------------------------------------------------------
# 12d. THE STATUS BAR HAS A HIERARCHY, IN PIXELS.
#
# Sections 5 and 8 already check that the bar SAYS the right thing - they grep
# `msg=undo invert` off stdout. That is a different claim from this one: stdout
# proves the string was computed, and says nothing whatever about what reached the
# window. The bar could be painted in one flat colour, or off the bottom edge, and
# every existing status check would still pass. It was in fact painted flat: all
# fourteen fields at 200,200,212, so "what did that just do?" - the only reason a
# status bar exists - carried the same weight as the document size, which has not
# changed since the file was opened.
#
# Now split by LIFETIME: persistent STATE dim (150,150,164, 6.29:1 on the
# 20,20,25 bar), the transient MESSAGE bright (238,238,246, 15.91:1) behind a
# 1px rule. Both clear WCAG AA. The state is deliberately the dimmer of the two -
# a hierarchy needs a quiet level in order to have a loud one.
#
# The three probes are a state glyph, the rule, and a message glyph, all under the
# same script so the string is the one being measured.
out=$(run 'click:+L,hsv:200:0.9:1,brush:26:0.5:1,tool:brush,stroke:40:40:210:210,winpx:70:744,winpx:323:744,winpx:341:745')
check "status STATE is the dim tier"          'winpx(70,744)=150,150,164'  "$out"
check "a rule separates state from message"   'winpx(323,744)=58,58,70'    "$out"
check "the MESSAGE is the bright tier"        'winpx(341,745)=238,238,246' "$out"
# The two tiers must differ - a regression to one flat colour would pass a check
# that only named one of them, so the contrast between the two is the real claim:
# 150,150,164 vs 238,238,246 is 2.53:1, comfortably distinguishable.

# THE RULE IS PLACED FROM THE MEASURED TEXT WIDTH, not a fixed column - the state
# string grows when a selection appears. Same script plus a selection, so the
# state text is LONGER: the rule must have moved right, and x=323 (the rule above)
# must now be something else. A hardcoded column would fail this.
out=$(run 'click:+L,tool:mrect,marquee:10:10:60:60,winpx:323:744')
check "the rule moves when the state text grows" 'winpx(323,744)=20,20,25' "$out"

# ---------------------------------------------------------------------------
# 12e. THE LAYER ROW'S BLEND AND OPACITY ARE TWO COLUMNS, NOT ONE STRING.
#
# They used to be one right-aligned concatenation, "normal 100%" / "multiply 75%".
# Right-aligning a concatenation aligns only its outer edge: measured off the
# window, four rows ended together at x=1168 but STARTED at 1098, 1098, 1101,
# 1098, because the join floats by the difference in blend-name widths. So neither
# fact formed a column, and comparing blend modes down the stack - most of what a
# layers panel is FOR - meant reading each row individually.
#
# The default document is the right fixture precisely because its two rows
# disagree in both fields: "blue" is multiply 75% at y=348 and "ramp" is normal
# 100% at y=372. Under the old single-string layout the two rows' text began at
# different x; if that regressed, the gutter check below is what fails.
#
# THE GUTTER IS THE LOAD-BEARING CHECK. x=1134 is between the two columns, and it
# must be bare row band on BOTH rows - 52,74,96 on the selected row 0 and
# 40,40,48 on row 1. A single joined string puts a space or a glyph there on at
# least one row, because the join lands wherever the blend name happens to end.
out=$(run 'winpx:1101:347,winpx:1123:372,winpx:1134:347,winpx:1134:372')
check "row 0 blend column has its glyphs"    'winpx(1101,347)=150,160,175' "$out"
check "row 1 blend column has its glyphs"    'winpx(1123,372)=150,160,175' "$out"
check "a gutter separates the columns (row 0)" 'winpx(1134,347)=52,74,96'  "$out"
check "a gutter separates the columns (row 1)" 'winpx(1134,372)=40,40,48'  "$out"
# Measured column edges afterwards, for the record: the blend column ends at
# 1127/1126 and the opacity column at 1168/1168 on the two rows. Both are shared
# edges now; before, only the outer one was.

# ---------------------------------------------------------------------------
# 12f. THE TOOLBAR'S MARGINS ARE SYMMETRIC.
#
# The toolbar's 21 gaps used to be SIX different values - 2, 4, 6, 10, 12, 14 -
# with no rule relating them, so the same 2px meant "these two are one stepper" in
# one place and "these are unrelated commands" in another. Four independent filters
# were spaced exactly like the two halves of a -/+ pair. Space is the cheapest
# grouping signal a toolbar has and it was being spent at random.
#
# Now three values with one meaning each: TB_TIGHT 3 (one unit), TB_PAIR 8
# (stepper pairs within a group), TB_GROUP 16 (between groups). Roughly a
# doubling, so they are told apart by ratio rather than by the 4-vs-6 that read as
# noise. Measured off the rendered toolbar afterwards, the histogram is exactly
# {3:12, 8:3, 16:6} over 22 controls.
#
# WHAT IS CHECKED HERE, and why it is the margins rather than the histogram: the
# gap VALUES are a property of layout.wyn and are asserted in test_layout.wyn,
# where they can be stated as ratios. What no layout test can see is whether the
# accumulated walk actually LANDS where it should - the toolbar is built by adding
# widths and gaps 22 times, and an error anywhere in that chain shows up only as a
# right margin that does not match the left.
#
# This caught a real error: the hand-computed path-entry width left a right margin
# of 3 against a left margin of 8. The width is 190, not the 195 the arithmetic
# said, and it was found by measuring the window.
out=$(run 'winpx:5:12,winpx:8:12,winpx:1171:12,winpx:1174:12')
check "the toolbar's left margin is bare"    'winpx(5,12)=38,38,46'     "$out"
check "the first control starts at PAD"      'winpx(8,12)=120,120,140'  "$out"
check "the last control ends at WIN_W - PAD" 'winpx(1171,12)=120,120,140' "$out"
check "the toolbar's right margin is bare"   'winpx(1174,12)=38,38,46'  "$out"
# 8 on the left and 8 on the right, over an accumulated walk of 22 controls.

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
