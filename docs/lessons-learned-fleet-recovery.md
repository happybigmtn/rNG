# Lessons Learned — RNG Fleet Recovery

This doc summarizes a real-world fleet recovery incident and the repeatable process we now use.

## Symptoms
- Fleet stuck at a low height
- Internal miner repeatedly logs “no templates / timeout waiting for first template”
- Nodes appear online but chain doesn’t advance

## Root cause
- Internal miner was gated behind `IsInitialBlockDownload()`.
- IBD can remain latched when the chain tip isn’t considered “recent enough” (e.g., `-maxtipage` / stalled chain). This creates a deadlock: no blocks → no recent tip → IBD stays true → miner refuses to mine.

## Fix
- Allow mining even during IBD.
- Support RandomX VM init for light mode.

(See commit `6c76f543` in this repo.)

## Build/distribution lesson
- Nix-built binaries may not run on Ubuntu/WSL if the ELF interpreter points into `/nix/store/...`.
- For typical VPS miners: use official release tarballs or build on Ubuntu (or an Ubuntu container).

## Fleet reset (Option A)
- Wipe datadirs and re-mine from genesis.
- Use RandomX light mode for low-resource mining (`-minerandomx=light -minethreads=1`).

### Fork convergence tip
If you start many miners at once from genesis, you will get temporary forks. For fastest convergence:
1) mine on 1–2 nodes
2) let the rest sync (`-connect=<canonical-peer>`)
3) enable mining broadly once `getbestblockhash` matches across nodes
