#!/usr/bin/env bash
cd "$(dirname "$0")"

WALLET="bc1qrs70rjscarwa9n8fj698f00sz0sf4xn228vdx7"
WORKER="Dream001"
POOL="stratum+tcp://eu.hashmonkeys.cloud:12363"
PASSWORD="x"
THREADS="8"

./bobminer \
  -o "$POOL" \
  -u "$WALLET.$WORKER" \
  -p "$PASSWORD" \
  -t "$THREADS"
