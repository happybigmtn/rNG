RNG
===

RNG is a Bitcoin Core-derived node for the live RNG network. The current tree is
based on Bitcoin Core `30.2` and keeps RNG's existing RandomX proof-of-work,
RNG network parameters, and operator fleet tooling.

The daemon and CLI are built as `rngd` and `rng-cli`. See the platform-specific
build guides in [doc](/doc) for the inherited Bitcoin Core build system.

What Changed In The 30.2 Port
-----------------------------

This port refreshes RNG on top of Bitcoin Core `30.2` while preserving the
network's RandomX block proof-of-work. RandomX protects block mining against
specialized mining hardware pressure; it does not make transaction signatures
post-quantum by itself.

The port also adds an operator-only QSB transaction path. In this repository,
QSB support means:

- `contrib/qsb/` can build the current `rng-qsb-v1-toy` funding and spend
  templates.
- `-enableqsboperator` enables local-only RPCs for queuing supported QSB
  transactions directly to an operator miner.
- Public relay policy remains unchanged; ordinary peers still reject these
  transactions as non-standard, and that is intentional.
- QSB support is a narrow escape hatch for the supported template family, not a
  claim that every RNG transaction is quantum-safe by default.

The merge branch keeps the inherited Windows, macOS, Linux, GUI, fuzz, and unit
test CI surface active so the `30.2` port is validated as a full Bitcoin
Core-derived node, not just as a daemon-only build.

Operational details are in [doc/qsb-operations.md](doc/qsb-operations.md), the
builder documentation is in [contrib/qsb/README.md](contrib/qsb/README.md), and
the implementation/rollout state is preserved in [EXECPLAN.md](EXECPLAN.md) and
[PLANS.md](PLANS.md).

Live Fleet State
----------------

As of 2026-04-10, the Bitcoin Core `30.2` plus QSB build has been deployed to
`contabo-validator-01`, `contabo-validator-02`, `contabo-validator-04`, and
`contabo-validator-05`. The non-canary validators finished sync, the temporary
`minimumchainwork=0` catch-up override was removed, mining was re-enabled, and
all four validators reported the same active tip with empty QSB queues.

The detailed rollout log, including binary hashes, service overrides, canary
funding/spend transaction IDs, rollback notes, and merge-readiness state, is in
[EXECPLAN.md](EXECPLAN.md).

What Is Bitcoin Core?
---------------------

Bitcoin Core connects to a Bitcoin-style peer-to-peer network to download and
fully validate blocks and transactions. RNG inherits that node architecture,
wallet code, GUI build option, RPC surface, and test framework while changing
network-specific pieces such as chain parameters and proof-of-work.

Further information about the inherited Bitcoin Core codebase is available in
the [doc folder](/doc).

License
-------

Bitcoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/license/MIT.

Development Process
-------------------

The `master` branch is regularly built (see `doc/build-*.md` for instructions) and tested, but it is not guaranteed to be
completely stable. [Tags](https://github.com/bitcoin/bitcoin/tags) are created
regularly from release branches to indicate new official, stable release versions of Bitcoin Core.

The https://github.com/bitcoin-core/gui repository is used exclusively for the
development of the GUI. Its master branch is identical in all monotree
repositories. Release branches and tags do not exist, so please do not fork
that repository unless it is for development reasons.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md)
and useful hints for developers can be found in [doc/developer-notes.md](doc/developer-notes.md).

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled during the generation of the build system) with: `ctest`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python.
These tests can be run (if the [test dependencies](/test) are installed) with: `build/test/functional/test_runner.py`
(assuming `build` is your build directory).

The CI (Continuous Integration) systems make sure that every pull request is tested on Windows, Linux, and macOS.
The CI must pass on all commits before merge to avoid unrelated CI failures on new pull requests.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

Changes to translations as well as new translations can be submitted to
[Bitcoin Core's Transifex page](https://explore.transifex.com/bitcoin/bitcoin/).

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.
