#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
echo "Installing BobMiner deps for UserLAnd Ubuntu..."
sudo apt update
sudo apt install -y build-essential clang make git curl libssl-dev libjansson-dev
echo "Building BobMiner..."
make clean || true
make -j"$(nproc)"
chmod +x start.sh
echo
echo "Done. Edit wallet with: nano start.sh"
echo "Then run: ./start.sh"
