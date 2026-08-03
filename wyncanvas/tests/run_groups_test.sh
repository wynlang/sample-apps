#!/bin/bash
# Layer groups, from the UI, through the real editor binary.
#
# tests/test_layer.wyn proves the STORE keeps a valid tree and test_render.wyn
# proves a group composites correctly. Neither can prove the two commands a user
# actually has are wired to them, or that undo puts the document back - src/ui.wyn
# is a program with a `main` and an event queue and cannot be imported.
#
# So this drives WYNCANVAS_SCRIPT and asserts on the printed tree. The `tree`
# action prints one row per layer with its kind and parent, which is the whole
# document structure - a check against a single index would pass while the rest of
# the tree was wrong.
#
#   ./tests/run_groups_test.sh          # from the repo root
set -uo pipefail
cd "$(dirname "$0")/.."

# Compiler lookup (SKIPs cleanly when there is none) and the bounded runner that
# makes a hang impossible - see tests/lib_ui_test.sh for why both are shared.
. tests/lib_ui_test.sh
ui_test_init

PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ui_run deletes the stale src/ui.wyn.out (which would ignore every edit to
# ui.wyn or an imported module) and bounds the run so it cannot hang.
run_script() {
    ui_run 4 "$1" | grep -E '^  (tree|group|ungroup|key)|^  TIMEOUT'
}

echo "=== Running layer-groups UI test ==="

# The document opens with two raster layers, and the active one is the top.
OUT=$(run_script 'tree')
if [ "$(printf '%s' "$OUT" | grep -c '^  tree ')" = "2" ]; then
  ok "the document starts as two flat layers"
else
  bad "unexpected starting document"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# ---- grouping puts the group BELOW its child -------------------------------
#
# Which is not a detail: set_parent refuses a parent above its child, and the
# render walk composites a group by scanning upward for rows that name it. A
# command that inserted the group above would silently fail to group anything.
OUT=$(run_script 'group,tree')
if printf '%s' "$OUT" | grep -q 'tree 1 group.*kind=2 parent=-1'; then
  ok "the new group is at index 1, top-level"
else
  bad "group not where expected"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
if printf '%s' "$OUT" | grep -q 'tree 2 .*parent=1'; then
  ok "the active layer moved INTO the group (parent=1)"
else
  bad "the layer was not parented"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
if printf '%s' "$OUT" | grep -q 'tree 0 .*parent=-1'; then
  ok "the untouched bottom layer stays top-level"
else
  bad "the bottom layer was disturbed"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# ---- ungroup is the exact inverse ------------------------------------------
OUT=$(run_script 'group,ungroup,tree')
if printf '%s' "$OUT" | grep -q 'tree 2 .*parent=-1'; then
  ok "ungroup returns the layer to the top level"
else
  bad "ungroup did not detach"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
# The group ROW remains - ungroup detaches a layer, it does not delete the group.
# An empty group is a documented no-op at render time, so leaving it is safe and
# is what a user expects from an "ungroup this layer" command.
if printf '%s' "$OUT" | grep -q 'tree 1 group.*kind=2'; then
  ok "the group row survives an ungroup (it is now empty)"
else
  bad "the group row vanished"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# Ungrouping something that is not in a group must say so rather than corrupt.
OUT=$(run_script 'ungroup,tree')
if printf '%s' "$OUT" | grep -q 'not in a group'; then
  ok "ungrouping a top-level layer is refused with a message"
else
  bad "no refusal message"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# ---- UNDO restores the flat document ---------------------------------------
#
# The parent assignment is deliberately NOT in the undo log: the recorded entry is
# the group's INSERT, and `remove` orphans a deleted group's children - so undo
# reaches the right state through a mechanism that already existed. This is the
# check that the reasoning holds in practice.
OUT=$(run_script 'group,key:z+cmd,tree')
if [ "$(printf '%s' "$OUT" | grep -c '^  tree ')" = "2" ]; then
  ok "undo removes the group row (back to two layers)"
else
  bad "undo left the wrong layer count"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
if [ "$(printf '%s' "$OUT" | grep -c 'parent=-1')" = "2" ]; then
  ok "undo leaves BOTH layers top-level, none stranded in a dead group"
else
  bad "a layer still points at a removed group"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
if printf '%s' "$OUT" | grep -q 'kind=2'; then
  bad "a group row survived the undo"
else
  ok "no group row survives the undo"
fi

# ---- nesting works from the UI too -----------------------------------------
# Grouping twice puts the layer two levels deep, which exercises the recursive
# render path from a user's gesture rather than from a unit test.
OUT=$(run_script 'group,group,tree')
if [ "$(printf '%s' "$OUT" | grep -c 'kind=2')" = "2" ]; then
  ok "grouping twice makes two groups"
else
  bad "expected two group rows"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
# The inner group must itself be inside the outer one, not a sibling.
if printf '%s' "$OUT" | grep -qE 'tree 2 group.*parent=1'; then
  ok "the second group nests inside the first"
else
  bad "the groups are siblings, not nested"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# ---- the keyboard shortcut reaches the same handlers -----------------------
# Cmd+G / Cmd+Shift+G. The collision that matters is BARE g, which is the bucket
# fill: a shortcut table that checked the letter before the modifier would make
# picking the fill tool group a layer instead.
OUT=$(run_script 'key:g+cmd,tree')
if printf '%s' "$OUT" | grep -q 'kind=2'; then
  ok "cmd+G creates a group"
else
  bad "cmd+G did not group"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
OUT=$(run_script 'key:g+cmd,key:g+cmd+shift,tree')
if printf '%s' "$OUT" | grep -q 'tree 2 .*parent=-1'; then
  ok "cmd+shift+G ungroups"
else
  bad "cmd+shift+G did not ungroup"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
OUT=$(run_script 'key:g,tree')
if printf '%s' "$OUT" | grep -q 'kind=2'; then
  bad "BARE g grouped a layer - it must select the bucket fill"
else
  ok "bare g is still the bucket fill, not group"
fi

# ---- THE PANEL SHOWS THE TREE ----------------------------------------------
#
# Groups were invisible in the layer panel: a flat list, so a grouped layer looked
# like a sibling of its own group. The panel now indents by depth and a group with
# children carries a twisty.
#
# The coordinates below are derived once, from the layout functions, rather than
# guessed: LAYERS_Y = VIEW_Y+12+SV_SIZE+92 = 46+12+150+92 = 300, ROW_H = 24, and
# PANEL_X = WIN_W-PANEL_W = 1180-250 = 930. A depth-0 row band starts at x=936 and
# a depth-1 row starts 12px further right, at 948 - so x=938 is INSIDE a depth-0
# row and OUTSIDE a depth-1 one, which is the whole discrimination.
panel_script() {
    ui_run 5 "$1" | grep -E '^  (row|rows|winpx|clickat|fold|unfold)|^  TIMEOUT'
}

OUT=$(panel_script 'group,rows')
if printf '%s' "$OUT" | grep -q 'row 0 .*depth=1'; then
  ok "the grouped layer is drawn INDENTED (depth=1)"
else
  bad "no indented row"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
if printf '%s' "$OUT" | grep -q 'rows drawn=3 of 3'; then
  ok "all three rows are drawn while the group is open"
else
  bad "wrong open row count"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# INDENTATION IS VISIBLE IN PIXELS, not just in a number. x=938 on the top row is
# the row's own band when that row is at depth 0, and panel background once the row
# has been indented - which no state assertion can distinguish.
OUT=$(panel_script 'rows,winpx:938:310')
FLAT=$(printf '%s' "$OUT" | sed -n 's/.*winpx(938,310)=\(.*\)/\1/p')
OUT=$(panel_script 'group,rows,winpx:938:310')
IND=$(printf '%s' "$OUT" | sed -n 's/.*winpx(938,310)=\(.*\)/\1/p')
if [ -n "$FLAT" ] && [ -n "$IND" ] && [ "$FLAT" != "$IND" ]; then
  ok "the indent is VISIBLE: x=938 changes colour once grouped ($FLAT -> $IND)"
else
  bad "indent not visible in pixels (flat=${FLAT:-none} indented=${IND:-none})"
fi

# ---- collapsing hides the children -----------------------------------------
OUT=$(panel_script 'group,fold:1,rows')
if printf '%s' "$OUT" | grep -q 'rows drawn=2 of 3'; then
  ok "collapsing a group removes its child from the panel (3 -> 2 rows)"
else
  bad "fold did not hide the child"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
OUT=$(panel_script 'group,fold:1,unfold:1,rows')
if printf '%s' "$OUT" | grep -q 'rows drawn=3 of 3'; then
  ok "expanding brings it back"
else
  bad "unfold did not restore"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# THE PAINTER ITSELF SKIPS THE HIDDEN ROW, checked in pixels. The `rows` action
# above reports what the panel SHOULD draw by applying the same row_hidden rule; it
# cannot prove the painter applied it. Measured by mutation: deleting the painter's
# skip left every check above green. The third row band (y = 300 + 2*24 + 10 = 358)
# is a drawn row while the group is open and bare panel once it is folded.
OUT=$(panel_script 'group,rows,winpx:960:358')
OPENPX=$(printf '%s' "$OUT" | sed -n 's/.*winpx(960,358)=\(.*\)/\1/p')
OUT=$(panel_script 'group,fold:1,rows,winpx:960:358')
FOLDPX=$(printf '%s' "$OUT" | sed -n 's/.*winpx(960,358)=\(.*\)/\1/p')
if [ -n "$OPENPX" ] && [ -n "$FOLDPX" ] && [ "$OPENPX" != "$FOLDPX" ]; then
  ok "the PAINTER stops drawing the folded row ($OPENPX -> $FOLDPX)"
else
  bad "the third row band did not change (open=${OPENPX:-none} folded=${FOLDPX:-none})"
fi

# ---- A CLICK LANDS ON THE ROW THAT WAS DRAWN -------------------------------
#
# The painter and the click-target loop are two separate walks, and after a fold they
# agree only if BOTH derive y from the count of DRAWN rows. Placing targets by layer
# index instead - which is what the code did before - puts every target where the row
# would be if nothing were folded, so a click selects a layer the user cannot see.
# That is invisible to every other check in this file.
#
# Row 1 (y = 300 + 24 + 10 = 334) is the GROUP while open, and `ramp` once folded.
OUT=$(panel_script 'group,rows,clickat:1050:334')
if printf '%s' "$OUT" | grep -q 'clickat(1050,334) -> active=1'; then
  ok "open: clicking visible row 1 selects the group (layer 1)"
else
  bad "open-row click selected the wrong layer"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
OUT=$(panel_script 'group,fold:1,clickat:1050:334')
if printf '%s' "$OUT" | grep -q 'clickat(1050,334) -> active=0'; then
  ok "FOLDED: the same pixel now selects layer 0, following the painter"
else
  bad "folded-row click did not follow the painter"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# ---- the twisty folds; the name selects ------------------------------------
# Two meanings in one row, split by x. The twisty is checked BEFORE the visibility
# box because on a group row those two columns would otherwise overlap.
OUT=$(panel_script 'group,clickat:938:334,rows')
if printf '%s' "$OUT" | grep -q 'collapsed'; then
  ok "clicking the twisty column collapses the group"
else
  bad "twisty click did not fold"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi
OUT=$(panel_script 'group,clickat:1050:334')
if printf '%s' "$OUT" | grep -q 'active: group'; then
  ok "clicking the NAME selects the group instead of folding it"
else
  bad "name click did not select"; printf '%s\n' "$OUT"|sed 's/^/        /'
fi

# ---- and the document still renders ----------------------------------------
# A structural change that broke compositing would leave every check above green.
OUT=$(run_script 'group,px:32:32')
if printf '%s' "$OUT" | grep -qE 'px\(32,32\)=[0-9]'; then
  ok "the grouped document still composites to a pixel"
else
  # px: prints through a different prefix, so re-run without the grep filter.
  RAW=$(ui_run 4 'group,px:32:32' | grep -E 'px\(|^  TIMEOUT')
  if printf '%s' "$RAW" | grep -qE 'px\(32,32\)=[0-9]'; then
    ok "the grouped document still composites to a pixel"
  else
    bad "no pixel came back after grouping"; printf '%s\n' "$RAW"|sed 's/^/        /'
  fi
fi

# A killed run is a failure even if no assertion above happened to notice.
TIMEOUTS=$(ui_test_timeouts)
if [ "$TIMEOUTS" -gt 0 ]; then
  echo "  FAIL  $TIMEOUTS run(s) were killed for exceeding the time bound"
  FAIL=$((FAIL+TIMEOUTS))
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "groups: $PASS pass, 0 fail"
  exit 0
fi
echo "groups: $PASS pass, $FAIL fail"
exit 1
