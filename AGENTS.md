# GCOS Agent Notes

- Start with `docs/dev-brief.md` or `make context` before broad repo reads.
- Keep the project identity as a portable TUI Mini Agent OS implementing the upstream `cocojisoo/GarbageCollector_OS` brief.
- No GUI/app bundle path. Do not reintroduce SDL, Pango, Cairo, LaunchServices, or `.app` packaging.
- Plain TUI use must stay local simulation only. External token-using prefixes and raw `[CODEX:...]` are blocked in TUI mode.
- Keep unsafe OS actions behind typed policy gates; no free-form root/kernel execution.
- Verify normal changes with `make check` and `make portable-check`.
