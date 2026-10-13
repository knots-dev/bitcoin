# DATUM-ready Bitcoin Knots

This directory helps you run Bitcoin Knots as the template-building node behind the
[DATUM Gateway](https://github.com/OCEAN-xyz/datum_gateway). With DATUM, **you** run the node and
build the block templates; a DATUM-supporting pool (for example OCEAN) only coordinates how the
reward is split. The gateway fetches the pool's payout outputs and places them in the coinbase of
templates built from your own node, and submits solved blocks straight to the network.

## What Knots does for you out of the box

| Setting | Default | Why |
|---|---|---|
| `-blockmaxsize` / `-blockmaxweight` | `3985000` | Leaves ~15 kB of the block free for the pool's generation (payout) transaction. DATUM requires this reservation. |
| `-maxmempool` | `1000` (MB) | Larger mempool, as recommended by the gateway. |
| `-blockreconstructionextratxn` | `1000000` | Better compact block reconstruction. |
| `-blockreconstructionextratxnsize` | `100` (MB) | Size cap for the extra-transaction cache so the count above is usable. |

Settings that cannot have safe defaults are provided as files here:

* `bitcoin.conf.example` – RPC credentials for the gateway and `blocknotify`.
* `datum_gateway_config.json.example` – matching gateway configuration.
* `setup-datum.sh` – generates both files with a random RPC password.

## Quick start

```sh
contrib/datum/setup-datum.sh <your-payout-address> ~/datum
cat ~/datum/bitcoin.conf.datum >> ~/.bitcoin/bitcoin.conf   # then restart bitcoind
datum_gateway -c ~/datum/datum_gateway_config.json
```

Point your ASICs at the gateway's stratum port (`23334` in the example). Pool connection details
(host, port, pool public key) come from your pool's DATUM documentation; set them in the `datum`
section of the gateway config as described in the gateway repository. The node itself never talks
to the pool.

## Built-in CPU miner (`setgenerate`)

Knots again has a built-in CPU miner, intended for regtest/signet/testnet and experiments:

```sh
bitcoin-cli setgenerate true 4 <address>   # 4 threads, reward to <address>
bitcoin-cli gethashespersec
bitcoin-cli setgenerate false
```

or at startup: `-gen -genproclimit=4 -miningaddress=<address>`.

On mainnet a CPU has effectively no chance of finding a block. Use the DATUM Gateway (or another
`getblocktemplate` miner) for real mining.
