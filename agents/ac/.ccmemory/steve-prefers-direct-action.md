---
name: steve-prefers-direct-action
description: Steve wants direct investigation/verification over theorizing, and obvious fixes done — not offered as multiple-choice menus
metadata:
  type: feedback
tags: [feedback, working-style, steve, communication]
---

Steve reacts sharply to two things: (1) theorizing/speculating about a problem instead of going and verifying it against the running system or code, and (2) being handed multiple-choice option menus when the right fix is obvious.

**Why:** he's the system's author and debugs empirically; hedging and option-dumps waste his time and read as not having done the work.

**How to apply:** When something's broken, investigate concretely (read the actual code, query the live agent, run the command) and state findings as fact, not hypotheses. When a fix is clearly correct, recommend it directly and offer to just do it — don't fan out a 3-way AskUserQuestion. Reserve questions for genuine forks where his answer changes the outcome. Be terse and technical (matches his global no-platitudes rule).
