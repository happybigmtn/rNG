# Security Policy

## Supported Versions

RNG currently treats the most recent published release tag as the supported production line.

- Latest tagged release: supported for security fixes and public mining installs
- Older tags: unsupported unless explicitly called out in release notes
- `main`: development only; it may contain unreleased consensus, mining, or bootstrap changes

## Reporting a Vulnerability

For embargoed vulnerabilities, use GitHub's private vulnerability reporting flow for this repository:

- <https://github.com/happybigmtn/rng/security/advisories/new>

If private reporting is unavailable for any reason, do not open a public issue first. Use the repository security tab or contact the maintainer through a private channel and wait for acknowledgement before disclosing details publicly.

For non-sensitive bugs, broken docs, or mining support issues, use the public issue tracker:

- <https://github.com/happybigmtn/rng/issues>

## Scope

The highest-risk RNG-specific surfaces are:

- RandomX proof-of-work validation
- internal miner startup, shutdown, and RPC control paths
- chain bootstrap and release asset integrity
- chain identity constants in `src/kernel/chainparams.cpp`

Security-sensitive changes to those areas should ship with:

- a tagged release
- updated release notes or security review notes
- targeted test coverage where practical
