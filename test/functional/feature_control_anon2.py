#!/usr/bin/env python3
# Copyright (c) 2021 Ghost Core Team
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_particl import GhostTestFramework

from test_framework.util import (
    assert_raises_rpc_error,
    assert_equal
)


class ControlAnonTest(GhostTestFramework):
    def set_test_params(self):
         # Start three nodes both of them with anon enabled
        self.setup_clean_chain = True
        self.num_nodes = 2

        self.extra_args = [['-debug', '-anonrestricted=0', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1', ] for i in range(self.num_nodes)]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self, split=False):
        self.add_nodes(self.num_nodes, extra_args=self.extra_args)
        self.start_nodes()
        self.connect_nodes_bi(0, 1)
        self.sync_all()

    def run_test(self):
        nodes = self.nodes
        self.import_genesis_coins_a(nodes[0])
        self.import_genesis_coins_a(nodes[1])

        node1_receiving_addr = nodes[1].getnewstealthaddress()
        anon_tx_txid0 = nodes[0].sendparttoanon(node1_receiving_addr, 100, '', '', False, 'node0 -> node1 p->a')
        assert anon_tx_txid0 != ""
        print("ANON TXID " + anon_tx_txid0)
        self.stakeBlocks(4)
        self.wait_for_mempool(nodes[0], anon_tx_txid0)
        self.wait_for_mempool(nodes[1], anon_tx_txid0)
        
        self.stop_nodes()

        self.start_node(0, ['-wallet=default_wallet', '-debug', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])
        self.start_node(1, ['-wallet=default_wallet', '-debug', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])
        # This is just so that node 0 can stake

        wi_1 = nodes[1].getwalletinfo()
        wi_2 = nodes[0].getwalletinfo()
        print(wi_1)
        print(wi_2)

        # address = "pX9N6S76ZtA5BfsiJmqBbjaEgLMHpt58it"
        coincontrol = {'inputs': [{'tx': anon_tx_txid0, 'n': 1}]}
        receiving_addr = nodes[1].getnewaddress()

        unspent_filtered = nodes[1].listunspentanon(1, 9999, [node1_receiving_addr])
        print(unspent_filtered)
        anon_tx = nodes[1].sendtypeto('anon', 'part', [{'address': receiving_addr, 'amount': 50}, ], '', '', 1, 1, False)

        print(nodes[1].getwalletinfo())
        print(nodes[0].getwalletinfo())

        # Now trying to spend to the recovery address
        nodes[0].importprivkey("7shnesmjFcQZoxXCsNV55v7hrbQMtBfMNscuBkYrLa1mcJNPbXhU")
        recovery_addr = "pX9N6S76ZtA5BfsiJmqBbjaEgLMHpt58it"

        txid = nodes[1].sendtypeto('anon', 'part', [{'address': recovery_addr, 'amount': 20}, ], '', '', 1, 1, False)

        recovery_addr_info = nodes[0].getwalletinfo()
        print("Balance : ")
        print(recovery_addr_info["balance"])
        assert recovery_addr_info["balance"] == 0.995*60


if __name__ == '__main__':
    ControlAnonTest().main()
