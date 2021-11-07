#!/usr/bin/env python3
# Copyright (c) 2021 Ghost Core Team
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_particl import GhostTestFramework

from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error
)


class ControlAnonTest3(GhostTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-debug', '-anonrestricted=0', '-lastanonindex=0', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1', ] for i in range(self.num_nodes)]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self, split=False):
        self.add_nodes(self.num_nodes, extra_args=self.extra_args)
        self.start_nodes()
        self.connect_nodes_bi(0, 1)
        self.sync_all()

    def init_nodes_with_anonoutputs(self, nodes, node1_receiving_addr, node0_receiving_addr, ring_size):
        anon_tx_txid0 = nodes[0].sendtypeto('ghost', 'anon', node1_receiving_addr, 600, '', '', False, 'node0 -> node1 p->a')
        self.wait_for_mempool(nodes[0], anon_tx_txid0)
        self.stakeBlocks(3)

        unspent_filtered_node1 = nodes[1].listunspentanon(0, 9999, [node1_receiving_addr])

        while True:
            unspent_fil_node0 = nodes[0].listunspentanon(0, 9999, [node0_receiving_addr])
            if len(unspent_fil_node0) < ring_size * len(unspent_filtered_node1):
                nodes[0].sendparttoanon(node0_receiving_addr, 1000, '', '', False, 'node0 -> node1 p->a')
                self.stakeBlocks(4)
            else:
                break

    def restart_nodes_with_anonoutputs(self):
        nodes = self.nodes
        self.stop_nodes()
        self.start_node(0, ['-wallet=default_wallet', '-debug', '-anonrestricted=0', '-lastanonindex=0'])
        self.start_node(1, ['-wallet=default_wallet', '-debug', '-anonrestricted=0', '-lastanonindex=0'])
        self.connect_nodes_bi(0, 1)
        node1_receiving_addr = nodes[1].getnewstealthaddress()
        node0_receiving_addr = nodes[0].getnewstealthaddress()
        self.init_nodes_with_anonoutputs(nodes, node1_receiving_addr, node0_receiving_addr, 3)
        self.stop_nodes()

        self.start_node(0, ['-wallet=default_wallet', '-debug'])
        self.start_node(1, ['-wallet=default_wallet', '-debug'])
        self.connect_nodes_bi(0, 1)
        self.sync_all()

    def run_test(self):
        nodes = self.nodes
        self.import_genesis_coins_a(nodes[0])
        self.import_genesis_coins_b(nodes[1])
     
        # Restart the nodes with some anon index
        self.restart_nodes_with_anonoutputs()
        self.stop_nodes()

        # Node 0 has its lastanonindex defaulted to 0
        self.start_node(0, ['-wallet=default_wallet', '-debug', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])
        # Node 1 has its anon index set to 100
        self.start_node(1, ['-wallet=default_wallet', '-debug', '-lastanonindex=100'
                            '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])

        self.connect_nodes_bi(0, 1)
        receiving_addr = nodes[0].getnewaddress()

        # Sending a transaction from anon to ghost being inside node 0 will succeed
        bad_anon_tx_txid = nodes[0].sendtypeto('anon', 'ghost', [{'address': receiving_addr, 'amount': 15, 'subfee': True}])
        assert_equal(self.wait_for_mempool(nodes[0], bad_anon_tx_txid), True)

        self.stakeBlocks(1, 0, False)
        # The transaction succeeded inside node 0, but it won't inside node 1 because of the bad anon index
        rawtx_bad_anon_txid = nodes[0].getrawtransaction(bad_anon_tx_txid)
        assert_raises_rpc_error(None, "bad-anonin-extract-i", nodes[1].sendrawtransaction, rawtx_bad_anon_txid)

        # Retrieves the block which the bad_anon_tx_txid is inside and try to submit the block to node 0
        rawtx_block = nodes[0].getblock(nodes[0].getbestblockhash())
        assert bad_anon_tx_txid in rawtx_block['tx']

        node0_block_hex = nodes[0].getblock(rawtx_block["hash"], 0)
        assert_equal(nodes[1].submitblock(node0_block_hex), "duplicate-invalid")

if __name__ == '__main__':
    ControlAnonTest3().main()
