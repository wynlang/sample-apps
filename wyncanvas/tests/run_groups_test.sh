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

WYN="${WYN:-$WYN_ROOT/wyn}"
if [ ! -x "$WYN" ]; then
    echo "SKIP: no wyn binary (set WYN or WYN_ROOT)"
    exit 0
fi

PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# A stale src/ui.wyn.out ignores every edit to ui.wyn or an imported module.
run_script() {
    rm -f src/ui.wyn.out src/ui.wyn.c
    SDL_VIDEODRIVER=dummy WYNCANVAS_FRAMES=4 WYNCANVAS_SCRIPT="$1" \
        "$WYN" run src/ui.wyn 2>&1 | grep -E '^  (tree|group|ungroup|key)'
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

# ---- and the document still renders ----------------------------------------
# A structural change that broke compositing would leave every check above green.
OUT=$(run_script 'group,px:32:32')
if printf '%s' "$OUT" | grep -qE 'px\(32,32\)=[0-9]'; then
  ok "the grouped document still composites to a pixel"
else
  # px: prints through a different prefix, so re-run without the grep filter.
  rm -f src/ui.wyn.out
  RAW=$(SDL_VIDEODRIVER=dummy WYNCANVAS_FRAMES=4 WYNCANVAS_SCRIPT='group,px:32:32' \
        "$WYN" run src/ui.wyn 2>&1 | grep -E 'px\(')
  if printf '%s' "$RAW" | grep -qE 'px\(32,32\)=[0-9]'; then
    ok "the grouped document still composites to a pixel"
  else
    bad "no pixel came back after grouping"; printf '%s\n' "$RAW"|sed 's/^/        /'
  fi
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "groups: $PASS pass, 0 fail"
  exit 0
fi
echo "groups: $PASS pass, $FAIL fail"
exit 1
