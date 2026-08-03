# md-toc

Build a real, pasteable table of contents for a markdown file -- and tell you what is
structurally wrong with the document while you are there.

- **Skips fenced code blocks.** A `# comment` inside a ` ```bash ` block is not a
  heading. The previous version of this app counted them, which is why its output
  nested `python/` under `Web server`.
- **Normalises depth** to the shallowest heading, so a document that starts at `##`
  is not indented pointlessly.
- **GitHub-style anchor slugs**, faithful to GitHub's actual rule -- lowercase, keep
  `[a-z0-9_-]`, drop punctuation and emoji, spaces to dashes, and do *not* collapse
  runs of dashes -- so every line is a working `[text](#slug)` you can paste back in.
- **Checks the structure**: a skipped level (`##` straight to `####`), duplicate
  headings whose anchors would collide, and more than one H1.

## Run

```bash
wyn run src/main.wyn              # defaults to README.md in the current directory
wyn run src/main.wyn sample.md    # or any file you name
```

`sample.md` is a deliberately imperfect document that trips all three checks:

```
  sample.md 8 headings, H1-H4

  - [Widget API](#widget-api)
    - [Getting Started](#getting-started)
        - [Requirements](#requirements)
    - [Usage](#usage)
      - [Basic Usage](#basic-usage)
      - [Basic Usage](#basic-usage)
    - [FAQ / Troubleshooting 🚀](#faq--troubleshooting-)
  - [Appendix](#appendix)

  structure
  H1 ██ 2
  H2 ███ 3
  H3 ██ 2
  H4 █ 1

  3 structural problems
  ! duplicate anchor #basic-usage x2 - links will land on the first one
  ! line 17: H2 jumps to H4 ("Requirements") - a level was skipped
  ! 2 H1 headings - a document should have one title
```

## Test

```bash
wyn test tests/test_toc.wyn
```

## Notes on the Wyn used here

`struct` + `[Heading]`, `.map` / `.filter` / `.min` / `.max` over structs, `HashMap`
for the anchor tally, and `String.chars()` for the slugifier.

Two compiler quirks the source works around, both noted in comments:

- `var heads: [Heading] = parse(...)` -- the element type of a struct array returned
  from a function is lost without the annotation, and `h.depth` then fails to compile
  (`member reference base type 'long long' is not a structure or union`).
- `.split("\n")`, not `.lines()` -- `.lines()` drops blank lines, which would make the
  reported line numbers wrong.
