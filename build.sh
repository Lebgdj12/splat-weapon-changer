#!/usr/bin/env bash
# build.sh - Setup devkitPro + build the SplatWeaponChanger.wps plugin
#
# Usage:
#   ./build.sh setup   # one-shot: installs devkitPro + WUT + WUPS on Linux/WSL
#   ./build.sh build   # compiles the plugin
#   ./build.sh all     # setup + build
#
# Tested on Debian 12/13, Ubuntu 22.04+, WSL2 Ubuntu.

set -euo pipefail

DEVKITPRO_ROOT="${DEVKITPRO:-/opt/devkitpro}"
PLUGIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

print_help() {
    cat <<EOF
Splat Weapon Changer — build script

Usage:
  $0 setup    Install devkitPro (requires sudo)
  $0 build    Compile the .wps plugin
  $0 all      setup then build
  $0 clean    Remove build artifacts

EOF
}

die() { echo "ERROR: $*" >&2; exit 1; }

check_sudo() {
    if ! sudo -n true 2>/dev/null; then
        die "Cette étape nécessite sudo. Lance le script avec les droits admin."
    fi
}

# ----------------------------------------------------------------------------
# setup: installs devkitPro via the official .deb installer
# ----------------------------------------------------------------------------
do_setup() {
    check_sudo
    echo "=== 1. Téléchargement du paquet devkitPro pacman ==="
    local tmpdeb; tmpdeb="$(mktemp --suffix=.deb)"
    curl -fL -o "$tmpdeb" https://apt.devkitpro.org/install-devkitpro.deb

    echo "=== 2. Installation du paquet (sudo) ==="
    sudo apt install -y "$tmpdeb"
    rm -f "$tmpdeb"

    echo "=== 3. dkp-pacman : update + install wiiu-dev / devkitPPC / wut ==="
    sudo dkp-pacman -Syu --noconfirm
    sudo dkp-pacman -S --noconfirm \
        devkitPPC \
        wiiu-dev \
        wiiu-portlibs \
        wut \
        general-tools \
        ppc-portlibs

    echo "=== 4. Clone de WiiUPluginSystem (WUPS) ==="
    if [ ! -d "$DEVKITPRO_ROOT/wups" ]; then
        sudo mkdir -p "$DEVKITPRO_ROOT/wups"
        sudo chown -R "$USER":"$USER" "$DEVKITPRO_ROOT/wups"
        git clone --recursive https://github.com/wiiu-env/WiiUPluginSystem.git "$DEVKITPRO_ROOT/wups"
    else
        echo "WUPS déjà présent, skip."
    fi

    echo "=== Setup terminé. Lance maintenant : $0 build ==="
}

# ----------------------------------------------------------------------------
# build: compile the plugin
# ----------------------------------------------------------------------------
do_build() {
    command -v powerpc-eabi-g++ >/dev/null 2>&1 \
        || die "devkitPPC pas trouvé. Lance d'abord : $0 setup"

    cd "$PLUGIN_DIR"
    echo "=== Compilation de SplatWeaponChanger.wps ==="
    make -j"$(nproc)" "$@"
    echo
    echo "==> Si tout est OK, tu as maintenant : $PLUGIN_DIR/SplatWeaponChanger.wps"
    echo "==> Copie ce fichier vers : sd:/wiiu/environments/aroma/plugins/SplatWeaponChanger.wps"
}

do_clean() {
    cd "$PLUGIN_DIR"
    make clean
}

# ----------------------------------------------------------------------------
case "${1:-}" in
    setup) do_setup ;;
    build) shift; do_build "$@" ;;
    all)   do_setup; do_build ;;
    clean) do_clean ;;
    ""|-h|--help) print_help ;;
    *) die "Commande inconnue : $1"; print_help ;;
esac
