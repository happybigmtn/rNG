#!/usr/bin/env bash
# RNG installer
# Works on Linux, macOS, and WSL.
# Usage: download and inspect install.sh, then run it locally

set -euo pipefail

RELEASE_VERSION="${RNG_VERSION:-}"
SOURCE_REF="${RNG_SOURCE_REF:-main}"
INSTALL_DIR="${RNG_INSTALL_DIR:-$HOME/.local/bin}"
DATA_DIR="${RNG_DATA_DIR:-$HOME/.rng}"
REPO="happybigmtn/rng"
GITHUB_URL="https://github.com/$REPO"
BOOTSTRAP_BASE_HASH="2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb"
BOOTSTRAP_BASE_HEIGHT="15091"
CHAIN_BUNDLE_ARCHIVE="rng-mainnet-15244-datadir.tar.gz"
BOOTSTRAP_HEADER_WAIT_SECONDS="${RNG_BOOTSTRAP_HEADER_WAIT_SECONDS:-900}"
TEMP_SOURCE_ROOT=""
TEMP_RELEASE_ROOT=""
SOURCE_DIR=""
HELPER_SCRIPTS_INSTALLED=0

PUBLIC_SEEDS=(
    "95.111.239.142:8433"
    "161.97.114.192:8433"
    "185.218.126.23:8433"
    "185.239.209.227:8433"
)

FORCE=0
ADD_PATH=0
NO_VERIFY=0
NO_CONFIG=0
BOOTSTRAP=0
SKIP_DEPS=0

usage() {
    cat <<'EOF'
RNG Installer

Usage: ./install.sh [flags]

Flags:
  --force       Reinstall even if rngd is already present
  --add-path    Add the install dir to PATH in your shell rc
  --bootstrap   Load the bundled assumeutxo snapshot after install
  --skip-deps   Do not auto-install build dependencies
  --no-verify   Skip checksum verification for binary releases
  --no-config   Do not create ~/.rng/rng.conf
  -h, --help    Show this help

Environment variables:
  RNG_VERSION      Optional tagged release to install from GitHub releases
  RNG_SOURCE_REF   Git ref to build from source (default: main)
  RNG_INSTALL_DIR  Install directory (default: ~/.local/bin)
  RNG_DATA_DIR     Data directory (default: ~/.rng)
  RNG_BOOTSTRAP_HEADER_WAIT_SECONDS  Max wait for snapshot base header (default: 900)

Examples:
  ./install.sh --add-path
  ./install.sh --add-path --bootstrap
  RNG_VERSION=vX.Y.Z ./install.sh --force
  RNG_SOURCE_REF=main ./install.sh
EOF
}

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --force) FORCE=1 ;;
            --add-path) ADD_PATH=1 ;;
            --bootstrap) BOOTSTRAP=1 ;;
            --skip-deps) SKIP_DEPS=1 ;;
            --no-verify) NO_VERIFY=1 ;;
            --no-config) NO_CONFIG=1 ;;
            -h|--help) usage; exit 0 ;;
            *) error "Unknown argument: $1 (use --help)" ;;
        esac
        shift
    done
}

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'

info() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

detect_platform() {
    OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
    ARCH="$(uname -m)"
    
    case "$OS" in
        linux*)
            if grep -qi microsoft /proc/version 2>/dev/null; then
                OS="windows-wsl"
            else
                OS="linux"
            fi
            ;;
        darwin*) OS="macos" ;;
        mingw*|msys*|cygwin*) OS="windows" ;;
        *) error "Unsupported OS: $OS" ;;
    esac
    
    case "$ARCH" in
        x86_64|amd64) ARCH="x86_64" ;;
        aarch64|arm64) ARCH="arm64" ;;
        *) error "Unsupported architecture: $ARCH" ;;
    esac
    
    PLATFORM="${OS}-${ARCH}"
    info "Detected platform: $PLATFORM"
}

check_binary_available() {
    case "$PLATFORM" in
        linux-x86_64|linux-arm64|macos-x86_64|macos-arm64)
            return 0 ;;
        windows-wsl-x86_64|windows-wsl-arm64)
            PLATFORM="linux-${ARCH}"
            return 0 ;;
        *)
            return 1 ;;
    esac
}

require_checksum_tool() {
    if [ "$NO_VERIFY" -eq 1 ]; then
        warn "Checksum verification disabled (--no-verify)"
        return 0
    fi
    if command -v sha256sum &>/dev/null || command -v shasum &>/dev/null; then
        return 0
    fi
    error "No checksum tool found (sha256sum/shasum). Install one or rerun with --no-verify."
}

verify_checksums() {
    if [ "$NO_VERIFY" -eq 1 ]; then
        warn "Skipping checksum verification"
        return 0
    fi
    
    info "Verifying checksums..."
    if command -v sha256sum &>/dev/null; then
        sha256sum -c SHA256SUMS || error "Checksum verification failed!"
    elif command -v shasum &>/dev/null; then
        shasum -a 256 -c SHA256SUMS || error "Checksum verification failed!"
    else
        error "No checksum tool found (unexpected)."
    fi
    success "Checksums verified"
}

cleanup() {
    if [ -n "$TEMP_RELEASE_ROOT" ]; then
        rm -rf "$TEMP_RELEASE_ROOT"
    fi
    if [ -n "$TEMP_SOURCE_ROOT" ]; then
        rm -rf "$TEMP_SOURCE_ROOT"
    fi
    return 0
}

trap cleanup EXIT

cpu_count() {
    nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 1
}

selected_source_ref() {
    if [ -n "$RELEASE_VERSION" ]; then
        printf '%s\n' "$RELEASE_VERSION"
    else
        printf '%s\n' "$SOURCE_REF"
    fi
}

install_binary() {
    [ -n "$RELEASE_VERSION" ] || return 1
    info "Downloading RNG $RELEASE_VERSION for $PLATFORM..."

    TARBALL="rng-${RELEASE_VERSION}-${PLATFORM}.tar.gz"
    URL="$GITHUB_URL/releases/download/${RELEASE_VERSION}/${TARBALL}"

    TEMP_RELEASE_ROOT=$(mktemp -d)
    cd "$TEMP_RELEASE_ROOT"

    if command -v curl &>/dev/null; then
        curl -fsSL -o "$TARBALL" "$URL" || return 1
    elif command -v wget &>/dev/null; then
        wget -q -O "$TARBALL" "$URL" || return 1
    else
        error "Neither curl nor wget found"
    fi
    require_checksum_tool
    tar -xzf "$TARBALL"
    cd release
    verify_checksums

    mkdir -p "$INSTALL_DIR"
    cp rngd rng-cli "$INSTALL_DIR/"
    chmod +x "$INSTALL_DIR/rngd" "$INSTALL_DIR/rng-cli"

    return 0
}

install_dependencies() {
    local compiler_found=0

    if [ "$SKIP_DEPS" -eq 1 ]; then
        warn "Skipping dependency installation (--skip-deps)"
        return
    fi

    if command -v c++ &>/dev/null || command -v g++ &>/dev/null || command -v clang++ &>/dev/null; then
        compiler_found=1
    fi

    if command -v cmake &>/dev/null && command -v git &>/dev/null && [ "$compiler_found" -eq 1 ]; then
        info "Existing build toolchain detected; skipping package-manager dependency install"
        return
    fi

    case "$OS" in
        linux|windows-wsl)
            if command -v apt-get &>/dev/null; then
                info "Installing dependencies via apt..."
                sudo apt-get update
                sudo apt-get install -y build-essential cmake git libboost-all-dev libssl-dev libevent-dev libsqlite3-dev
            elif command -v dnf &>/dev/null; then
                info "Installing dependencies via dnf..."
                sudo dnf install -y cmake gcc-c++ git boost-devel openssl-devel libevent-devel sqlite-devel
            elif command -v pacman &>/dev/null; then
                info "Installing dependencies via pacman..."
                sudo pacman -Sy --noconfirm cmake gcc git boost openssl libevent sqlite
            else
                error "Could not find package manager (apt/dnf/pacman)"
            fi
            ;;
        macos)
            if ! command -v brew &>/dev/null; then
                error "Homebrew not found. Install from https://brew.sh"
            fi
            info "Installing dependencies via Homebrew..."
            brew install cmake boost openssl@3 libevent sqlite
            ;;
    esac
}

install_helper_scripts() {
    if [ -z "$SOURCE_DIR" ] || [ ! -d "$SOURCE_DIR/scripts" ]; then
        return
    fi

    mkdir -p "$INSTALL_DIR"
    cp "$SOURCE_DIR/scripts/load-bootstrap.sh" "$INSTALL_DIR/rng-load-bootstrap"
    cp "$SOURCE_DIR/scripts/start-miner.sh" "$INSTALL_DIR/rng-start-miner"
    cp "$SOURCE_DIR/scripts/doctor.sh" "$INSTALL_DIR/rng-doctor"
    chmod +x "$INSTALL_DIR/rng-load-bootstrap" "$INSTALL_DIR/rng-start-miner" "$INSTALL_DIR/rng-doctor"
    HELPER_SCRIPTS_INSTALLED=1
    success "Installed helper commands rng-load-bootstrap, rng-start-miner, and rng-doctor"
}

prepare_source_tree() {
    local source_ref
    source_ref="$(selected_source_ref)"

    if [ -f "$PWD/CMakeLists.txt" ] && [ -f "$PWD/install.sh" ] && [ -d "$PWD/src" ]; then
        SOURCE_DIR="$PWD"
        info "Using local source tree: $SOURCE_DIR"
    else
        TEMP_SOURCE_ROOT=$(mktemp -d)
        SOURCE_DIR="$TEMP_SOURCE_ROOT/rng"
        info "Cloning repository at $source_ref..."
        git clone --depth 1 --branch "$source_ref" "$GITHUB_URL.git" "$SOURCE_DIR"
    fi

    if [ ! -f "$SOURCE_DIR/src/crypto/randomx/src/randomx.h" ]; then
        info "Fetching RandomX source..."
        rm -rf "$SOURCE_DIR/src/crypto/randomx"
        git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git "$SOURCE_DIR/src/crypto/randomx"
    fi
}

build_from_source() {
    info "Building from source (this may take 10-15 minutes)..."
    install_dependencies
    prepare_source_tree

    cd "$SOURCE_DIR"

    OPENSSL_FLAG=()
    if [ "$OS" = "macos" ]; then
        OPENSSL_FLAG=(-DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)")
    fi

    cmake -B build \
        -DBUILD_TESTING=OFF \
        -DENABLE_IPC=OFF \
        -DWITH_ZMQ=OFF \
        -DENABLE_WALLET=ON \
        "${OPENSSL_FLAG[@]}"
    cmake --build build -j"$(cpu_count)"

    mkdir -p "$INSTALL_DIR"
    cp build/bin/rngd build/bin/rng-cli "$INSTALL_DIR/"
    chmod +x "$INSTALL_DIR/rngd" "$INSTALL_DIR/rng-cli"

    success "Built and installed from source"
}

setup_config() {
    if [ "$NO_CONFIG" -eq 1 ]; then
        warn "Skipping config creation (--no-config)"
        return
    fi
    
    mkdir -p "$DATA_DIR"
    if [ -f "$DATA_DIR/rng.conf" ]; then
        warn "Config already exists at $DATA_DIR/rng.conf"
        return
    fi

    RPCPASS=$(openssl rand -hex 16 2>/dev/null || head -c 32 /dev/urandom | xxd -p | head -c 32)

    {
        cat <<EOF
# RNG live-mainnet config
# Public peers below are operator-run seed nodes for the current low-peer network.
# Current live genesis: 83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
server=1
daemon=1
rpcuser=agent
rpcpassword=$RPCPASS
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
minerandomx=fast
EOF
        for seed in "${PUBLIC_SEEDS[@]}"; do
            printf 'addnode=%s\n' "$seed"
        done
    } > "$DATA_DIR/rng.conf"

    success "Config created at $DATA_DIR/rng.conf"
}

setup_path() {
    if [[ ":$PATH:" == *":$INSTALL_DIR:"* ]]; then
        return
    fi
    warn "$INSTALL_DIR is not in your PATH"

    if [ "$ADD_PATH" -eq 1 ]; then
        SHELL_RC=""
        if [ -n "$BASH_VERSION" ] && [ -f "$HOME/.bashrc" ]; then
            SHELL_RC="$HOME/.bashrc"
        elif [ -n "$ZSH_VERSION" ] && [ -f "$HOME/.zshrc" ]; then
            SHELL_RC="$HOME/.zshrc"
        elif [ -f "$HOME/.profile" ]; then
            SHELL_RC="$HOME/.profile"
        fi
        
        if [ -n "$SHELL_RC" ]; then
            echo "export PATH=\"$INSTALL_DIR:\$PATH\"" >> "$SHELL_RC"
            success "Added $INSTALL_DIR to PATH in $SHELL_RC"
            info "Run: source $SHELL_RC"
        else
            info "Add to your shell config: export PATH=\"$INSTALL_DIR:\$PATH\""
        fi
    else
        info "Add to PATH: export PATH=\"$INSTALL_DIR:\$PATH\""
        info "Or rerun with --add-path to auto-configure"
    fi
}

verify_install() {
    export PATH="$INSTALL_DIR:$PATH"
    if ! command -v rngd &>/dev/null; then
        error "rngd not found after installation"
    fi

    info "Verifying installation..."
    "$INSTALL_DIR/rngd" --version
    success "RNG installed successfully!"
}

install_bootstrap_snapshot() {
    local source_snapshot dest_snapshot source_bundle dest_bundle

    source_snapshot="$SOURCE_DIR/bootstrap/rng-mainnet-15091.utxo"
    dest_snapshot="$DATA_DIR/bootstrap/rng-mainnet-15091.utxo"
    source_bundle="$SOURCE_DIR/bootstrap/$CHAIN_BUNDLE_ARCHIVE"
    dest_bundle="$DATA_DIR/bootstrap/$CHAIN_BUNDLE_ARCHIVE"

    if [ -n "$SOURCE_DIR" ] && [ -f "$source_snapshot" ]; then
        mkdir -p "$DATA_DIR/bootstrap"
        cp "$source_snapshot" "$dest_snapshot"
        chmod 644 "$dest_snapshot"
        success "Bundled snapshot copied to $dest_snapshot"
    fi
    if [ -n "$SOURCE_DIR" ] && [ -f "$source_bundle" ]; then
        mkdir -p "$DATA_DIR/bootstrap"
        cp "$source_bundle" "$dest_bundle"
        chmod 644 "$dest_bundle"
        success "Bundled chain bundle copied to $dest_bundle"
    fi
}

load_bootstrap() {
    local current_height snapshot_path bundle_path rpc_output current_headers header_wait_loops wait_step

    snapshot_path="$DATA_DIR/bootstrap/rng-mainnet-15091.utxo"
    bundle_path="$DATA_DIR/bootstrap/$CHAIN_BUNDLE_ARCHIVE"
    if [ ! -f "$bundle_path" ] && [ ! -f "$snapshot_path" ]; then
        warn "No bundled chain bundle or snapshot found under $DATA_DIR/bootstrap"
        return
    fi

    if [ -f "$bundle_path" ] && [ ! -d "$DATA_DIR/blocks" ] && [ ! -d "$DATA_DIR/chainstate" ]; then
        info "Extracting bundled chain bundle from $bundle_path..."
        mkdir -p "$DATA_DIR"
        tar -xzf "$bundle_path" -C "$DATA_DIR"
    fi

    info "Starting daemon for snapshot load..."
    "$INSTALL_DIR/rngd" -daemon -listen=0 -discover=0

    info "Waiting for RPC..."
    for _ in $(seq 1 30); do
        if rpc_output="$("$INSTALL_DIR/rng-cli" getblockcount 2>&1)"; then
            break
        fi
        case "$rpc_output" in
            *"Incorrect rpcuser or rpcpassword"*)
                warn "RPC credentials were rejected on the default port. Another rngd is probably already running on this machine; skipping bootstrap auto-load."
                return
                ;;
        esac
        sleep 1
    done

    if ! "$INSTALL_DIR/rng-cli" getblockcount >/dev/null 2>&1; then
        warn "RPC did not become ready; skipping bootstrap load"
        return
    fi

    current_height="$("$INSTALL_DIR/rng-cli" getblockcount)"
    if [ "$current_height" -gt 0 ]; then
        info "Datadir already has blocks at height $current_height; snapshot load is not needed"
        return
    fi

    if [ ! -f "$snapshot_path" ]; then
        warn "Bundled snapshot not found at $snapshot_path; leaving the extracted chain bundle in place"
        return
    fi

    info "Waiting for snapshot base header $BOOTSTRAP_BASE_HASH..."
    header_wait_loops=$((BOOTSTRAP_HEADER_WAIT_SECONDS / 2))
    if [ "$header_wait_loops" -lt 1 ]; then
        header_wait_loops=1
    fi

    for wait_step in $(seq 1 "$header_wait_loops"); do
        if "$INSTALL_DIR/rng-cli" getblockheader "$BOOTSTRAP_BASE_HASH" false >/dev/null 2>&1; then
            break
        fi
        if [ $((wait_step % 15)) -eq 0 ]; then
            current_headers="$("$INSTALL_DIR/rng-cli" getchainstates 2>/dev/null | sed -n 's/.*"headers"[[:space:]]*:[[:space:]]*\\([0-9][0-9]*\\).*/\\1/p' | head -1)"
            current_headers="${current_headers:-0}"
            info "Still waiting for snapshot base header; headers=$current_headers/$BOOTSTRAP_BASE_HEIGHT"
        fi
        sleep 2
    done

    if ! "$INSTALL_DIR/rng-cli" getblockheader "$BOOTSTRAP_BASE_HASH" false >/dev/null 2>&1; then
        warn "Snapshot base header is not available yet after ${BOOTSTRAP_HEADER_WAIT_SECONDS}s; let headers sync and rerun the load command later"
        return
    fi

    current_height="$("$INSTALL_DIR/rng-cli" getblockcount)"
    if [ "$current_height" -gt 0 ]; then
        warn "Datadir advanced to height $current_height before the snapshot could be loaded"
        warn "Stop the node, wipe blocks/chainstate, and rerun the snapshot load on a fresh datadir"
        return
    fi

    info "Loading bundled assumeutxo snapshot..."
    "$INSTALL_DIR/rng-cli" -rpcclienttimeout=0 loadtxoutset "$snapshot_path"
    success "Snapshot loaded"
}

print_next_steps() {
    local threads
    threads="$(cpu_count)"
    if [ "$threads" -gt 1 ]; then
        threads=$((threads - 1))
    fi

    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  RNG installed successfully!${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "Installed to: $INSTALL_DIR"
    echo ""
    echo "Next steps:"
    echo ""
    echo "  1. Start the daemon and verify the live genesis:"
    echo "     $INSTALL_DIR/rngd -daemon"
    echo "     sleep 10"
    echo "     $INSTALL_DIR/rng-cli getblockhash 0"
    echo "     # expected: 83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4"
    echo ""
    echo "  2. Check peer connectivity:"
    echo "     $INSTALL_DIR/rng-cli getconnectioncount"
    echo "     $INSTALL_DIR/rng-cli getblockchaininfo"
    echo "     # low peer counts are normal on the current operator-seeded network"
    echo ""
    if [ "$HELPER_SCRIPTS_INSTALLED" -eq 1 ]; then
        echo "  3. Run the built-in health check:"
        echo "     $INSTALL_DIR/rng-doctor"
        echo ""
        echo "  4. Fast miner setup:"
        echo "     $INSTALL_DIR/rng-start-miner"
        echo "     $INSTALL_DIR/rng-doctor"
        echo ""
    fi
    if [ -f "$DATA_DIR/bootstrap/$CHAIN_BUNDLE_ARCHIVE" ] || [ -f "$DATA_DIR/bootstrap/rng-mainnet-15091.utxo" ]; then
        echo "  5. Optional fast bootstrap from the bundled chain bundle or snapshot:"
        if [ "$HELPER_SCRIPTS_INSTALLED" -eq 1 ]; then
            echo "     $INSTALL_DIR/rng-load-bootstrap"
        fi
        echo "     # or manually wait until this succeeds before loading the snapshot:"
        echo "     $INSTALL_DIR/rng-cli getblockheader \"$BOOTSTRAP_BASE_HASH\" false"
        echo "     $INSTALL_DIR/rng-cli -rpcclienttimeout=0 loadtxoutset \"$DATA_DIR/bootstrap/rng-mainnet-15091.utxo\""
        echo "     $INSTALL_DIR/rng-cli getchainstates"
        echo ""
    fi
    echo "  6. Create a miner wallet and payout address manually:"
    echo "     $INSTALL_DIR/rng-cli createwallet \"miner\""
    echo "     ADDR=\$($INSTALL_DIR/rng-cli -rpcwallet=miner getnewaddress)"
    echo ""
    echo "  7. Restart with the internal miner manually:"
    echo "     $INSTALL_DIR/rng-cli stop"
    echo "     sleep 5"
    echo "     nice -n 19 $INSTALL_DIR/rngd -daemon -mine -mineaddress=\"\$ADDR\" -minethreads=$threads -minerandomx=fast"
    echo ""
    echo "  8. Monitor mining:"
    echo "     $INSTALL_DIR/rng-doctor"
    echo ""
    echo "Uninstall:"
    echo "  rm -rf $INSTALL_DIR/rng* $DATA_DIR"
    echo ""
}

main() {
    parse_args "$@"

    echo ""
    echo "╔══════════════════════════════════════════╗"
    echo "║  RNG Installer                           ║"
    echo "║  CPU-mineable for AI agents              ║"
    echo "╚══════════════════════════════════════════╝"
    echo ""

    if command -v rngd &>/dev/null && [ "$FORCE" -ne 1 ]; then
        INSTALLED_VERSION=$(rngd --version 2>/dev/null | head -1 || echo "unknown")
        success "RNG already installed: $INSTALLED_VERSION"
        info "Location: $(which rngd)"
        info "To reinstall, run: $0 --force"
        info "To uninstall: rm -rf $INSTALL_DIR/rng* $DATA_DIR"
        exit 0
    fi

    detect_platform

    if [ -n "$RELEASE_VERSION" ] && check_binary_available; then
        if install_binary; then
            success "Installed from binary release $RELEASE_VERSION"
        else
            warn "Binary download failed, building $RELEASE_VERSION from source..."
            build_from_source
        fi
    else
        info "Building live network source from $(selected_source_ref)"
        build_from_source
    fi

    setup_config
    install_bootstrap_snapshot
    install_helper_scripts
    setup_path
    verify_install
    if [ "$BOOTSTRAP" -eq 1 ]; then
        load_bootstrap
    fi
    print_next_steps
}

main "$@"
