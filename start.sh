#!/usr/bin/env bash
cd "$(dirname "$0")"

WALLET="bc1qrs70rjscarwa9n8fj698f00sz0sf4xn228vdx7"
WORKER="Dream001"
POOL="stratum+tcp://eu.hashmonkeys.cloud:12363"
PASSWORD="x"
THREADS="8"

# Colors
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
PURPLE='\033[1;35m'
CYAN='\033[1;36m'
WHITE='\033[1;37m'
DIM='\033[2m'
RESET='\033[0m'

clear

echo -e "${GREEN}"
cat <<'EOF'
========================================================================
                         B O B F A R M S
========================================================================
EOF

echo -e "${WHITE}"
cat <<'EOF'
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
EOF

echo -e "${GREEN}"
cat <<'EOF'
==========================================================
  B O B M I N E R   •   S H A 3 - 2 5 6 T   L E G I O N
==========================================================
EOF

echo -e "${RED}        STATUS : VILLAIN MODE ACTIVE${RESET}"
echo -e "${PURPLE}        FARM   : PHONE ARMY ONLINE${RESET}"
echo -e "${YELLOW}        HASH   : BC3 / SHA3-256T${RESET}"
echo -e "${RED}        MOTTO  : NO MERCY. NO SLEEP. JUST SHARES.${RESET}"

echo -e "${GREEN}========================================================================${RESET}"
echo
echo -e "${CYAN}Worker  :${RESET} ${WHITE}$WORKER${RESET}"
echo -e "${CYAN}Pool    :${RESET} ${WHITE}$POOL${RESET}"
echo -e "${CYAN}Threads :${RESET} ${WHITE}$THREADS${RESET}"
echo -e "${CYAN}Wallet  :${RESET} ${DIM}${WALLET:0:10}...${RESET}"
echo

./bobminer \
  -o "$POOL" \
  -u "$WALLET.$WORKER" \
  -p "$PASSWORD" \
  -t "$THREADS"
