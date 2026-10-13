#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Knots developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the built-in CPU miner (setgenerate / gethashespersec / -gen)."""

import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

ADDRESS = "bcrt1qw508d6qejxtdg4y5r3zarvary0c5xw7kygt080"


class SetGenerateTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [[], []]

    def run_test(self):
        node = self.nodes[0]

        self.log.info("getmininginfo reports the miner as stopped")
        info = node.getmininginfo()
        assert_equal(info["generate"], False)
        assert_equal(info["genproclimit"], 0)
        assert_equal(node.gethashespersec(), 0)

        self.log.info("setgenerate argument validation")
        assert_raises_rpc_error(-8, "No mining address", node.setgenerate, True)
        assert_raises_rpc_error(-5, "Invalid address", node.setgenerate, True, 1, "notanaddress")
        assert_raises_rpc_error(-8, "genproclimit", node.setgenerate, True, -2, ADDRESS)
        node.setgenerate(False)  # stopping when stopped is a no-op

        self.log.info("setgenerate true mines blocks paying to the address")
        node.setgenerate(True, 2, ADDRESS)
        info = node.getmininginfo()
        assert_equal(info["generate"], True)
        assert_equal(info["genproclimit"], 2)
        self.wait_until(lambda: node.getblockcount() >= 3, timeout=120)
        node.setgenerate(False)
        assert_equal(node.getmininginfo()["generate"], False)
        height = node.getblockcount()
        coinbase = node.getblock(node.getblockhash(1), 2)["tx"][0]
        assert_equal(coinbase["vout"][0]["scriptPubKey"]["address"], ADDRESS)

        self.log.info("the miner stays stopped once setgenerate false returned")
        time.sleep(1)
        assert_equal(node.getblockcount(), height)

        self.log.info("-gen starts the miner at startup using -miningaddress")
        self.restart_node(1, extra_args=["-gen", "-genproclimit=1", f"-miningaddress={ADDRESS}"])
        other = self.nodes[1]
        assert_equal(other.getmininginfo()["generate"], True)
        start = other.getblockcount()
        self.wait_until(lambda: other.getblockcount() > start, timeout=120)
        other.setgenerate(False)

        self.log.info("-gen without -miningaddress is rejected")
        self.stop_node(1)
        self.nodes[1].assert_start_raises_init_error(["-gen"], "Error: -gen requires -miningaddress=<addr> to be set")
        self.nodes[1].assert_start_raises_init_error(["-gen", "-miningaddress=bogus"], "Error: Invalid -miningaddress: 'bogus'")


if __name__ == '__main__':
    SetGenerateTest(__file__).main()
