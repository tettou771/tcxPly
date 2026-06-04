# tests

Headless behavioral tests for tcxPly, run automatically in CI. **You don't need
to run this by hand** — it's a console program that asserts the addon's
behavior (parsing, round-trips, typed property access, error paths) and exits
non-zero on failure.

CI (`TrussC-org/ci-actions`) builds and runs it on macOS / Windows / Linux.
To run it locally anyway:

```bash
trusscli update
trusscli run
```
