#!/bin/sh
# NAH Installation Script
# Usage: curl -fsSL https://raw.githubusercontent.com/rtorr/nah/main/install.sh | sh

set -e

REPO="rtorr/nah"
INSTALL_DIR="${NAH_INSTALL_DIR:-/usr/local/bin}"

# Detect OS and architecture
detect_platform() {
    OS="$(uname -s)"
    ARCH="$(uname -m)"

    case "$OS" in
        Linux)
            OS="linux"
            ;;
        Darwin)
            OS="darwin"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            OS="windows"
            ;;
        *)
            echo "Error: Unsupported operating system: $OS"
            exit 1
            ;;
    esac

    case "$ARCH" in
        x86_64|amd64)
            ARCH="x64"
            ;;
        arm64|aarch64)
            ARCH="arm64"
            ;;
        *)
            echo "Error: Unsupported architecture: $ARCH"
            exit 1
            ;;
    esac

    PLATFORM="${OS}-${ARCH}"
}

# Get the latest release version
get_latest_version() {
    if command -v curl >/dev/null 2>&1; then
        VERSION=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')
    elif command -v wget >/dev/null 2>&1; then
        VERSION=$(wget -qO- "https://api.github.com/repos/${REPO}/releases/latest" | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')
    else
        echo "Error: curl or wget is required"
        exit 1
    fi

    if [ -z "$VERSION" ]; then
        echo "Error: Could not determine latest version"
        exit 1
    fi
}

# Download and install
install_nah() {
    ARCHIVE="nah-${PLATFORM}.tar.gz"
    URL="https://github.com/${REPO}/releases/download/${VERSION}/${ARCHIVE}"

    echo "Installing NAH ${VERSION} for ${PLATFORM}..."

    # Create temp directory
    TMP_DIR=$(mktemp -d)
    trap 'rm -rf "$TMP_DIR"' EXIT

    # Download
    echo "Downloading ${URL}..."
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$URL" -o "${TMP_DIR}/${ARCHIVE}"
    else
        wget -q "$URL" -O "${TMP_DIR}/${ARCHIVE}"
    fi

    # Extract
    echo "Extracting..."
    tar -xzf "${TMP_DIR}/${ARCHIVE}" -C "$TMP_DIR"

    # Install
    if [ -w "$INSTALL_DIR" ]; then
        mv "${TMP_DIR}/nah" "$INSTALL_DIR/nah"
    else
        echo "Installing to ${INSTALL_DIR} (requires sudo)..."
        sudo mv "${TMP_DIR}/nah" "$INSTALL_DIR/nah"
    fi

    chmod +x "$INSTALL_DIR/nah"

    echo ""
    echo "NAH ${VERSION} installed successfully!"
    echo ""
    echo "Run 'nah --help' to get started."
}

main() {
    detect_platform
    get_latest_version
    install_nah
}

main
