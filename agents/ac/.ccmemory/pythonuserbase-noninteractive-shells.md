---
name: pythonuserbase-noninteractive-shells
description: ~/.local python tools (ccmemory, ccloop, ccusage-statusline) need PYTHONUSERBASE=$HOME/.local; set it in ~/.zshenv not ~/.zshrc
metadata:
  type: reference
tags: [python, pythonuserbase, zshenv, statusline, hooks, ccmemory, ccloop]
---

Steve's Python CLI tools (ccmemory, ccloop, ccusage-statusline) are installed under `~/.local/lib/python/site-packages` (unversioned user-site). They only import correctly when `PYTHONUSERBASE=$HOME/.local` is exported so Python's user-site resolves there; otherwise macOS defaults user-site to `~/Library/Python/3.14/...` and imports fail with `ModuleNotFoundError`.

The export lives in `~/.zshrc`, which is sourced **only for interactive shells**. Claude Code runs hooks (ccloop, ccmemory guard) and the statusLine command (`/Users/steve/.local/bin/ccusage-statusline`) in **non-interactive** shells, which don't source ~/.zshrc → all three fail. Symptom seen 2026-06-10: repeated `ModuleNotFoundError: No module named 'ccloop'/'ccmemory'/'statusline'` and a blank statusline.

Fix: move `export PYTHONUSERBASE="$HOME/.local"` from ~/.zshrc to **~/.zshenv** (sourced by every zsh, interactive or not). Alternatives: add `"env": {"PYTHONUSERBASE": "/Users/steve/.local"}` to ~/.claude/settings.json (Claude-only), or prefix the statusLine command with the var (statusline-only). Note the statusLine config already uses the full script path — the break is the module import inside it, not $PATH.
