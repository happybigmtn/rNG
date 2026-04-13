# LEARNINGS

## Review Queue Truthfulness

- Queue review must verify documented "verified facts" against live source even when
  the queued item is mostly documentation. In this pass, stale protocol-version,
  tail-emission, QSB merge-state, and sharepool skeleton claims survived in specs
  after later implementation commits changed the code.
- When a later item lands foundational scaffolding, revisit older planning specs
  that say "no code exists." Otherwise future review items inherit a false
  baseline and can incorrectly treat missing follow-up work as already absent or
  already complete.
