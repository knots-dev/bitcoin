Mining
------

- Defaults are now suited to running behind the [DATUM Gateway](https://github.com/OCEAN-xyz/datum_gateway):
  `-blockmaxsize` and `-blockmaxweight` default to 3985000 (leaving room for the pool's generation
  transaction), `-maxmempool` to 1000, `-blockreconstructionextratxn` to 1000000 and
  `-blockreconstructionextratxnsize` to 100 (MB). Nodes that want the previous behaviour can set
  `-blockmaxsize=300000 -blockmaxweight=1200000 -maxmempool=300 -blockreconstructionextratxn=32768
  -blockreconstructionextratxnsize=10`. See `contrib/datum` for configuration examples.
