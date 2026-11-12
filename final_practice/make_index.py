"""Rebuild final_practice/README.md from the question folders (python3 make_index.py)."""
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
GROUPS = [
    (0, 0, "Format example"),
    (1, 20, "Write a program in the course language"),
    (21, 45, "Add an instruction or new syntax"),
    (46, 60, "Instrumentation: counters, traces, reports, options"),
    (61, 75, "Memory: paging, frames, page tables, protection, TLB"),
    (76, 95, "OS: scheduling, shell commands, processes, synchronisation"),
    (96, 100, "OpenMP inside the simulator"),
]


def question_text(readme):
    """First paragraph of the '## Question' section, on one line."""
    m = re.search(r"^## Question\s*\n(.*?)(?:\n\s*\n|\n## )", readme, re.S | re.M)
    if not m:
        return ""
    text = " ".join(line.strip() for line in m.group(1).splitlines())
    text = text.replace("|", "\\|")
    return text if len(text) <= 220 else text[:217].rsplit(" ", 1)[0] + "…"


rows = []
for d in sorted(glob.glob(os.path.join(HERE, "q[0-9][0-9][0-9]_*"))):
    name = os.path.basename(d)
    num = int(name[1:4])
    path = os.path.join(d, "README.md")
    if not os.path.exists(path):
        continue
    readme = open(path, encoding="utf-8").read()
    first = readme.splitlines()[0] if readme else ""
    m = re.match(r"#\s*Q\d{3}\s*[—-]\s*(.*)", first)
    title = (m.group(1) if m else name).replace("|", "\\|")
    answer = "solution.patch" if os.path.exists(os.path.join(d, "solution.patch")) else "solution.txt"
    rows.append((num, name, title, question_text(readme), answer))

out = [
    "# Final lab test — 100 practice questions with tested answers",
    "",
    "Questions in the style of the CS527 lab tests: write a program for the mini-computer,",
    "or extend the simulator ([`../lab6/`](../lab6/)) with a new instruction, report, memory",
    "or OS feature. Each folder has the question, the answer (`solution.txt` or a small",
    "`solution.patch` against lab6), test programs and a `test.sh` that applies the answer to",
    "a fresh copy of lab6, builds it and checks the results.",
    "",
    "**How to use them, and how to attack an unseen question: [`GUIDE.md`](GUIDE.md).**",
    "",
    "```bash",
    "./run_all.sh                 # check every answer (applies, builds, runs)",
    "./q021_*/test.sh             # check one",
    "cd ~/exam/sim && patch -p1 < <repo>/final_practice/q021_*/solution.patch   # use one",
    "```",
    "",
]
for lo, hi, title in GROUPS:
    group = [r for r in rows if lo <= r[0] <= hi]
    if not group:
        continue
    out += [f"## Q{lo:03d}–Q{hi:03d}: {title}" if lo != hi else f"## {title}", "",
            "| # | Question | Folder |", "|---|---|---|"]
    for num, name, qtitle, text, answer in group:
        out.append(f"| Q{num:03d} | **{qtitle}**. {text} | [`{name}`]({name}/) |")
    out.append("")
open(os.path.join(HERE, "README.md"), "w", encoding="utf-8").write("\n".join(out).rstrip() + "\n")
print(f"{len(rows)} questions indexed")
