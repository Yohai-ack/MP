---
name: skl_prj_exportplan
description: Build the local export folder of the MP course (STM32 repo) from the files in git. Produces export/MP2026_plan.pdf and .docx, export/coursePolicies.pdf, export/assN/assN_instructions.pdf with each assignment's provided files, and export/tutorials/ with one PDF per tutorial section of the deck. Use when Yohai invokes /skl_prj_exportplan or asks to export, publish-prepare or regenerate the plan, assignment and tutorial PDFs.
---

# skl_prj_exportplan: build the export folder

Everything in `export/` is generated from files in git, so the folder is git-ignored and **rebuilt from scratch on every run**
(the script deletes `export/` first). Never put hand-made files there.

## What it produces

| Output | Source | Notes |
|---|---|---|
| `export/MP2026_plan.pdf` | `MP2026_plan.tex` | the full plan, red `\flag{}` notes **kept** (for Bucher and Yohai) |
| `export/MP2026_plan.docx` | the same, via `skl_glb_tex2Word`'s `tex2word.py` | made from a copy in `export/_build`, so the repo's own `MP2026_plan.docx` is never touched (CLAUDE.md rule) |
| `export/coursePolicies.pdf` | `\section{Course policies}` of the plan | red flags and yellow `\bnote{}` remarks **removed** (student-facing; same for the assignment PDFs) |
| `export/assN/assN_instructions.pdf` | `\subsection{Assignment N: ...}` of the plan, up to the next `\subsection` | red flags **removed**; one folder per assignment found in the plan |
| `export/assN/<files>` | the top-level files of `assignments/assN/` | the files students get; subfolders (`solution/`) are **never** copied |
| `export/tutorials/Tutorial1.pdf`, `Tutorial2.pdf`, ... | each section of `tutorials/MP_tutorials_2026.pptx` | named from the section's number ("Tutorial 3: ..." gives `Tutorial3.pdf`, "Session 13" gives `Session13.pdf`); hidden slides left out; a section with no slides is skipped |

Build files (LaTeX logs, the deck copy) go to `export/_build`, which is deleted at the end when everything
succeeded, and kept when something failed (to read the logs). `--keep-build` keeps it always.

## Steps

1. **Check the inputs are saved.** Unsaved edits in an editor are not exported. **Ask Yohai to save and close
   PowerPoint**: the tutorials are exported through PowerPoint, and a PowerPoint that holds a recovered
   ("[Autosaved]") or unsaved file, or shows a dialog, refuses to open the copy (E_FAIL). The script never closes
   Yohai's own PowerPoint; it quits PowerPoint only if it started it.
2. **Run the script** from the repo root:

   ```bash
   python ".claude/skills/skl_prj_exportplan/export_plan.py"
   ```

   Options: `--no-word` (skip the .docx), `--no-tutorials` (skip PowerPoint), `--keep-build`. It prints one `[OK]`, `[FAIL]` or `[SKIP]`
   line per output and a `DONE:` summary, and exits with 1 if anything failed.
3. **Check the result.**
   - Every line must be `[OK]` (or a `[SKIP]` you asked for). For a `[FAIL]` on a PDF, read the `!` error in
     `export/_build/<name>/<name>.log` and fix the cause in `MP2026_plan.tex` (usually an unescaped `_`, `#`, `%`, `&`).
   - The `.docx` line shows `TABLES=a/b LEAKS=n`: tables must match and leaks must be 0, as in `skl_glb_tex2Word`.
   - Open one student PDF (`export/ass1/ass1_instructions.pdf`) with `pymupdf` and confirm no red `[...]` notes are
     left and the headings look right.
   - Compare the list of tutorial PDFs with the deck's sections.
4. **Report** in a short table: each output, OK or not, and for `assN` the provided files that were copied. Remind Yohai
   that `export/` is local only (not in git) and that the PDFs for students still need a human read before publishing
   (the red flags are removed automatically, but blue "less important" text stays as normal text).

## Requirements

`pdflatex` on PATH, Python with `pypandoc_binary` and `python-docx` (for the .docx), Microsoft PowerPoint (for the
tutorials, through COM), and the global skill `skl_glb_tex2Word` (its `tex2word.py`).
