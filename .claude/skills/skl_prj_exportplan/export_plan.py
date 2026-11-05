"""Build the local export folder of the MP course from the files in git.

    python export_plan.py [--no-word] [--no-tutorials]

Writes into <repo>/export (git-ignored, rebuilt from scratch on every run):

    export/MP2026_plan.pdf            full plan (red flags kept: for Bucher and Yohai)
    export/MP2026_plan.docx           Word copy of the full plan (tex2word skill)
    export/coursePolicies.pdf         "Course policies" section only, red flags removed
    export/assN/assN_instructions.pdf one assignment section, red flags removed
    export/quizzes/quizN_en.pdf       one quiz, student version (no solutions), from the plan
    export/quizzes/quizN_he.pdf       the same in Hebrew, from the quiz's %HE lines in the plan (lualatex)
    export/assN/<provided files>      top-level files of assignments/assN (never solution/)
    export/tutorials/<one pdf per section of tutorials/MP_tutorials_2026.pptx>

Build files go to export/_build. Needs pdflatex, and PowerPoint (COM) for the tutorials.
The repo's own MP2026_plan.docx is never touched: tex2word runs on a copy in _build.
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]           # .claude/skills/<name>/export_plan.py
PLAN = REPO / "MP2026_plan.tex"
DECK = REPO / "tutorials" / "MP_tutorials_2026.pptx"
ASSIGNMENTS = REPO / "assignments"
OUT = REPO / "export"
BUILD = OUT / "_build"
TEX2WORD = Path.home() / ".claude" / "skills" / "skl_glb_tex2Word" / "tex2word.py"
COURSE = "Microprocessor-Based Product Design 2026/27"

report = []          # (item, status) lines printed at the end


def log(item, status):
    report.append((item, status))
    print(f"[{status}] {item}", flush=True)


# ---------------------------------------------------------------- LaTeX helpers
def split_plan(tex):
    """Return (preamble, body) of the plan: preamble = everything before \\begin{document}."""
    i = tex.index("\\begin{document}")
    j = tex.index("\\end{document}")
    return tex[:i], tex[i + len("\\begin{document}"):j]


def section_text(body, start_pat, end_pats):
    """Text from the line matching start_pat up to (not including) the first end_pat after it."""
    m = re.search(start_pat, body, re.M)
    if not m:
        return None
    rest = body[m.start():]
    ends = [re.search(p, rest[1:], re.M) for p in end_pats]
    ends = [e.start() + 1 for e in ends if e]
    return rest[:min(ends)] if ends else rest


def student_doc(preamble, content, top):
    """A standalone document: the plan's preamble, red flags removed, unnumbered headings.
    top = the heading level of the section in the plan ('section' or 'subsection'); it becomes
    \\section*, and the levels below it move up with it."""
    if top == "subsection":
        content = re.sub(r"\\subsubsection\*?\{", r"\\SUBSUB{", content)
        content = re.sub(r"\\subsection\*?\{", r"\\section*{", content)
        content = content.replace("\\SUBSUB{", "\\subsection*{")
    else:
        content = re.sub(r"\\section\{", r"\\section*{", content)
        content = re.sub(r"\\subsection\{", r"\\subsection*{", content)
    return (preamble
            + "\\renewcommand{\\flag}[1]{}   % red notes for Bucher and Yohai are not published\n"
            + "\\providecommand{\\bnote}[1]{}\\renewcommand{\\bnote}[1]{}   % nor Bucher's yellow remarks\n"
            + "\\begin{document}\n"
            + "\\begin{center}{\\large\\bfseries " + COURSE + "}\\end{center}\n"
            + content
            + "\n\\end{document}\n")


def strip_solutions(tex):
    """The student version of a quiz: no solution environments, no \\correct marks, and room to write
    where the source has \\answerspace."""
    tex = re.sub(r"\\begin\{solution\}.*?\\end\{solution\}\n?", "", tex, flags=re.S)
    tex = re.sub(r"\\correct\b", "", tex)
    return tex


def hebrew_lines(sec):
    """The Hebrew version kept in a quiz section of the plan: its lines start with "%HE " (comments for
    pdflatex and for the Word copy). Returns (the section without them, the Hebrew body or None)."""
    keep, he = [], []
    for line in sec.split("\n"):
        if line.startswith("%HE"):
            he.append(line[4:] if line.startswith("%HE ") else "")
        else:
            keep.append(line)
    return "\n".join(keep), ("\n".join(he) if he else None)


# the preamble of the Hebrew quizzes (lualatex); their text is only in the plan
HEBREW_PREAMBLE = r"""\documentclass[11pt,a4paper]{article}
\usepackage[margin=2cm]{geometry}
\usepackage[bidi=basic, layout=lists.tabular]{babel}
\babelprovide[main, import]{hebrew}
\babelprovide[import]{english}
\babelfont[hebrew]{rm}{David}
\usepackage{booktabs,enumitem,xcolor,listings}
\lstset{language=C, basicstyle=\ttfamily\small, columns=fullflexible, keepspaces=true, upquote=true, showstringspaces=false, frame=single, xleftmargin=1em, xrightmargin=1em}
\setlist{nosep}
\newcommand{\flag}[1]{\textcolor{red}{[#1]}}
\newcommand{\correct}{\quad\textcolor{green!50!black}{\textbf{[נכון]}}}
\newenvironment{solution}{\par\noindent\ignorespaces}{\par}
\newcommand{\answerspace}[1]{}
\newcommand{\en}[1]{\babelsublr{\foreignlanguage{english}{#1}}}
\newcommand{\code}[1]{\babelsublr{\foreignlanguage{english}{\texttt{#1}}}}
\newenvironment{codeblock}{\begin{otherlanguage}{english}}{\end{otherlanguage}}
"""


STUDENT_QUIZ = ("\\renewcommand{\\flag}[1]{}   % red notes for Bucher and Yohai are not published\n"
                "\\renewcommand{\\answerspace}[1]{\\vspace{#1}}   % room to write the answer\n")


def pdflatex(tex_path, out_pdf, engine="pdflatex"):
    """Compile twice in its own folder; copy the PDF to out_pdf. Returns True on success."""
    for _ in range(2):
        r = subprocess.run([engine, "-interaction=nonstopmode", "-halt-on-error", tex_path.name],
                           cwd=tex_path.parent, capture_output=True, text=True, errors="replace")
    pdf = tex_path.with_suffix(".pdf")
    if r.returncode != 0 or not pdf.exists():
        errs = [l for l in r.stdout.splitlines() if l.startswith("!")][:3]
        log(f"{out_pdf.relative_to(REPO)}  ({'; '.join(errs) or 'pdflatex failed'})", "FAIL")
        return False
    out_pdf.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(pdf, out_pdf)
    log(str(out_pdf.relative_to(REPO)), "OK")
    return True


# ---------------------------------------------------------------- export steps
def export_plan_and_sections(word):
    tex = PLAN.read_text(encoding="utf-8")
    preamble, body = split_plan(tex)

    # 1. the full plan, as it is
    d = BUILD / "plan"
    d.mkdir(parents=True)
    shutil.copy2(PLAN, d / PLAN.name)
    pdflatex(d / PLAN.name, OUT / "MP2026_plan.pdf")

    # 2. its Word copy, made from a copy in _build (the repo's .docx is never touched). The %HE lines
    #    (the Hebrew quizzes) are dropped first: they are comments for pdflatex, but pandoc reads the
    #    ones inside a lstlisting as code
    if word:
        if not TEX2WORD.exists():
            log("MP2026_plan.docx  (tex2word.py not found)", "FAIL")
        else:
            w = BUILD / "word"
            w.mkdir(parents=True)
            (w / PLAN.name).write_text("".join(l for l in tex.splitlines(keepends=True)
                                               if not l.startswith("%HE")), encoding="utf-8")
            r = subprocess.run([sys.executable, str(TEX2WORD), str(w / PLAN.name), "--overwrite"],
                               capture_output=True, text=True, errors="replace")
            docx = w / "MP2026_plan.docx"
            info = " ".join(l for l in r.stdout.splitlines() if l.startswith(("TABLES=", "LEAKS=")))
            if r.returncode == 0 and docx.exists():
                shutil.copy2(docx, OUT / "MP2026_plan.docx")
                log(f"export/MP2026_plan.docx  ({info})", "OK")
            else:
                log(f"MP2026_plan.docx  ({(r.stderr or r.stdout).strip()[-200:]})", "FAIL")
    else:
        log("MP2026_plan.docx  (skipped: --no-word)", "SKIP")

    # 3. course policies, red flags removed
    pol = section_text(body, r"^\\section\{Course policies\}", [r"^\\section\{"])
    if pol is None:
        log("coursePolicies.pdf  (no \\section{Course policies} in the plan)", "FAIL")
    else:
        d = BUILD / "coursePolicies"
        d.mkdir(parents=True)
        f = d / "coursePolicies.tex"
        f.write_text(student_doc(preamble, pol, "section"), encoding="utf-8")
        pdflatex(f, OUT / "coursePolicies.pdf")

    # 4. one file per assignment, red flags removed
    nums = sorted({int(n) for n in re.findall(r"^\\subsection\{Assignment (\d+)", body, re.M)})
    for n in nums:
        sec = section_text(body, rf"^\\subsection\{{Assignment {n}\b", [r"^\\subsection\{", r"^\\section\{"])
        d = BUILD / f"ass{n}"
        d.mkdir(parents=True)
        f = d / f"ass{n}_instructions.tex"
        f.write_text(student_doc(preamble, sec, "subsection"), encoding="utf-8")
        pdflatex(f, OUT / f"ass{n}" / f"ass{n}_instructions.pdf")

    # 5. one student version per quiz (no solutions), in English and in Hebrew, both from the plan:
    #    the Hebrew text is the quiz's "%HE " comment lines, compiled with lualatex
    for q in sorted({int(n) for n in re.findall(r"^\\subsection\{Quiz (\d+)\}", body, re.M)}):
        sec = section_text(body, rf"^\\subsection\{{Quiz {q}\}}", [r"^\\subsection\{", r"^\\section\{"])
        sec, he = hebrew_lines(sec)
        d = BUILD / f"quiz{q}"
        d.mkdir(parents=True)
        f = d / f"quiz{q}_en.tex"
        doc = student_doc(preamble + STUDENT_QUIZ, strip_solutions(sec), "subsection")
        f.write_text(doc, encoding="utf-8")
        pdflatex(f, OUT / "quizzes" / f"quiz{q}_en.pdf")

        if he is None:
            log(f"export/quizzes/quiz{q}_he.pdf  (Quiz {q} in the plan has no %HE lines)", "FAIL")
            continue
        f = d / f"quiz{q}_he.tex"
        f.write_text(HEBREW_PREAMBLE + STUDENT_QUIZ + "\\begin{document}\n" + strip_solutions(he)
                     + "\n\\end{document}\n", encoding="utf-8")
        pdflatex(f, OUT / "quizzes" / f"quiz{q}_he.pdf", engine="lualatex")
    return nums


def export_provided_files(nums):
    """Copy the top-level files of assignments/assN (the files students get). Subfolders such as
    solution/ are never copied."""
    for n in nums:
        src = ASSIGNMENTS / f"ass{n}"
        dst = OUT / f"ass{n}"
        dst.mkdir(parents=True, exist_ok=True)
        files = [p for p in src.glob("*") if p.is_file()] if src.exists() else []
        for p in files:
            shutil.copy2(p, dst / p.name)
        log(f"export/ass{n}/  provided files: {', '.join(p.name for p in files) or 'none'}", "OK")


PS_TUTORIALS = r"""
$ErrorActionPreference = 'Stop'
$deck = '%DECK%'; $out = '%OUT%'
$wasRunning = [bool](Get-Process POWERPNT -ErrorAction SilentlyContinue)   # Yohai's own PowerPoint
$app = New-Object -ComObject PowerPoint.Application
$pres = $app.Presentations.Open($deck, $true, $false, $false)
try {
  $res = @()
  $sp = $pres.SectionProperties
  for ($i = 1; $i -le $sp.Count; $i++) {
    $first = $sp.FirstSlide($i); $n = $sp.SlidesCount($i)
    $item = @{ name = $sp.Name($i); first = $first; count = $n; file = '' }
    if ($n -gt 0) {
      $pdf = Join-Path $out ('section_' + $i + '.pdf')
      $pres.PrintOptions.Ranges.ClearAll()
      $range = $pres.PrintOptions.Ranges.Add($first, $first + $n - 1)
      # PDF, screen quality, no frames, slides, hidden slides left out, slide range
      $pres.ExportAsFixedFormat($pdf, 2, 1, 0, 1, 1, 0, $range, 4)
      $item.file = $pdf
    }
    $res += $item
  }
  $res | ConvertTo-Json -Compress
} finally { $pres.Close(); if (-not $wasRunning) { $app.Quit() } }   # never close Yohai's PowerPoint
"""


def slug(text):
    return re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_")


def export_tutorials():
    d = BUILD / "tutorials"
    d.mkdir(parents=True)
    deck = d / ("exportcopy_" + DECK.name)  # a copy with another name: PowerPoint cannot open two
                                              # files of the same name, and the deck may be open
    shutil.copy2(DECK, deck)
    ps = PS_TUTORIALS.replace("%DECK%", str(deck)).replace("%OUT%", str(d))
    r = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", ps],
                       capture_output=True, text=True, errors="replace")
    if r.returncode != 0 or not r.stdout.strip():
        err = " ".join((r.stderr or r.stdout).split())
        running = "POWERPNT.EXE" in subprocess.run(["tasklist"], capture_output=True, text=True).stdout
        hint = ("PowerPoint is open and refused to open the copy (often a recovered or unsaved file, or a "
                "dialog). Save and close PowerPoint, then run again. " if running else "")
        log(f"tutorials  ({hint}PowerPoint export failed: {err[:300]})", "FAIL")
        return
    sections = json.loads(r.stdout.strip().splitlines()[-1])
    if isinstance(sections, dict):
        sections = [sections]
    dst = OUT / "tutorials"
    dst.mkdir(parents=True, exist_ok=True)
    for s in sections:
        m = re.match(r"\s*(Tutorial|Session)\s+(\d+)", s["name"], re.I)
        name = f"{m.group(1).capitalize()}{int(m.group(2))}" if m else slug(s["name"])   # Tutorial1, Session13
        if not s["file"]:
            log(f"export/tutorials/{name}.pdf  (section has no slides)", "SKIP")
            continue
        shutil.copy2(s["file"], dst / f"{name}.pdf")
        last = s["first"] + s["count"] - 1
        log(f"export/tutorials/{name}.pdf  (slides {s['first']} to {last})", "OK")


def main():
    word = "--no-word" not in sys.argv
    tutorials = "--no-tutorials" not in sys.argv
    if OUT.exists():
        shutil.rmtree(OUT)                     # fully generated: rebuilt from scratch
    BUILD.mkdir(parents=True)
    nums = export_plan_and_sections(word)
    export_provided_files(nums)
    if tutorials:
        export_tutorials()
    else:
        log("tutorials  (skipped: --no-tutorials)", "SKIP")
    fails = [r for r in report if r[1] == "FAIL"]
    if not fails and "--keep-build" not in sys.argv:
        shutil.rmtree(BUILD, ignore_errors=True)   # kept only when something failed, to read the logs
    print(f"\nDONE: {sum(r[1] == 'OK' for r in report)} OK, {len(fails)} FAIL, "
          f"{sum(r[1] == 'SKIP' for r in report)} SKIP. Output: {OUT}")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
