Mining
------

- The built-in CPU miner is back. `setgenerate true [genproclimit] [address]` starts it,
  `setgenerate false` stops it, `gethashespersec` reports its speed and `getmininginfo` gained
  `generate`, `genproclimit` and `hashespersec` fields. It can also be started at launch with
  `-gen`, `-genproclimit=<n>` and `-miningaddress=<addr>`. It is meant for regtest, signet and
  testnet; on mainnet use `getblocktemplate` with an external miner.
