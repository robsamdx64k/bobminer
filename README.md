# BobMiner Fresh v0.3

CPU-only BC3 / BitcoinIII SHA3-256t test miner for UserLAnd Ubuntu ARM64.

## Phone install

```bash
cd ~
rm -rf bobminer
git clone https://github.com/robsamdx64k/bobminer.git
cd bobminer
chmod +x install-userland.sh start.sh check-files.sh
./check-files.sh
./install-userland.sh
nano start.sh
./start.sh
```

Edit these in `start.sh`:

```bash
WALLET="PUT_BC3_WALLET_HERE"
WORKER="Dream001"
THREADS="8"
```

## Expected

The miner should connect, subscribe, authorize, receive jobs, show speed, and submit only stronger candidate shares.

## Notes

This is a test miner. If the pool says low difficulty share, the miner is hashing but the local share filter may need tuning. If it says malformed or invalid job/nonce, the stratum/header format needs patching.
