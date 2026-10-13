#!/usr/bin/env bash
# Copyright (c) 2026 The Bitcoin Knots developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Generate matching bitcoin.conf and datum_gateway_config.json files with a
# random RPC password so that the DATUM Gateway can talk to this node.
#
# Usage: contrib/datum/setup-datum.sh <payout-address> [output-dir]
#
# Nothing is overwritten: existing files are left alone and the script exits.
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <payout-address> [output-dir]" >&2
    exit 1
fi

ADDRESS="$1"
OUT="${2:-.}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! [[ "$ADDRESS" =~ ^[A-Za-z0-9]{14,90}$ ]]; then
    echo "Error: '$ADDRESS' does not look like a Bitcoin address" >&2
    exit 1
fi

mkdir -p "$OUT"
CONF="$OUT/bitcoin.conf.datum"
GW="$OUT/datum_gateway_config.json"
for f in "$CONF" "$GW"; do
    if [ -e "$f" ]; then
        echo "Error: $f already exists, refusing to overwrite" >&2
        exit 1
    fi
done

if command -v openssl >/dev/null 2>&1; then
    PASS="$(openssl rand -hex 24)"
else
    PASS="$(head -c 24 /dev/urandom | od -An -tx1 | tr -d ' \n')"
fi

umask 077
sed "s/^rpcpassword=CHANGE_ME$/rpcpassword=$PASS/" "$SRC/bitcoin.conf.example" > "$CONF"
sed -e "s/\"rpcpassword\": \"CHANGE_ME\"/\"rpcpassword\": \"$PASS\"/" \
    -e "s/CHANGE_ME_YOUR_BITCOIN_ADDRESS/$ADDRESS/" \
    "$SRC/datum_gateway_config.json.example" > "$GW"

echo "Wrote $CONF and $GW (mode 600)."
echo "1. Append the contents of $CONF to your bitcoin.conf and restart the node."
echo "2. Point datum_gateway at $GW (see contrib/datum/README.md)."
