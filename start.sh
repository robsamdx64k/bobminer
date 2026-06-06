#!/usr/bin/env bash
cd "$(dirname "$0")"

WALLET="bc1qrs70rjscarwa9n8fj698f00sz0sf4xn228vdx7"
WORKER="Dream001"
POOL="stratum+tcp://eu.hashmonkeys.cloud:12363"
PASSWORD="x"
THREADS="8"

clear

cat <<'EOF'
========================================================================
                         B O B F A R M S
========================================================================

                         .-""""""""""""-.
                      .-'  .-""""""-.    '-.
                    .'    /  .-..-.  \      '.
                   /     |  /  ||  \  |       \
                  ;      |  \  ||  /  |        ;
                  |       \  '-''-'  /         |
                  |        '--------'          |
                  |     .----.    .----.       |
                  |    /  BF  \__/  BF  \      |
                  |    \______/  \______/      |
                  |         .-""""""-.         |
                  |        /  ████    \        |
                  |        |  ████    |        |
                  |        \  ████    /        |
                  |         '-.____.-'         |
                  |       ___/||||||\___       |
                  |    .-'   MACHINE   '-.     |
                  |   /      GOD MODE      \    |
                  '._|____________________|_.'

========================================================================
        B O B M I N E R   •   S H A 3 - 2 5 6 T   L E G I O N
========================================================================
        STATUS : VILLAIN MODE ACTIVE
        FARM   : PHONE ARMY ONLINE
        HASH   : BC3 / SHA3-256T
        MOTTO  : NO MERCY. NO SLEEP. JUST SHARES.
========================================================================
EOF

echo "Worker   : $WORKER"
echo "Pool     : $POOL"
echo "Threads  : $THREADS"
echo "Algo     : sha3-256t
echo

./bobminer \
  -o "$POOL" \
  -u "$WALLET.$WORKER" \
  -p "$PASSWORD" \
  -t "$THREADS"
