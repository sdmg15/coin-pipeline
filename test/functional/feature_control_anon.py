#!/usr/bin/env python3
# Copyright (c) 2021 sdmg15
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_particl import GhostTestFramework
import os
import shutil

class ControlAnonTest(GhostTestFramework):
    def set_test_params(self):
         # Start two nodes both of them with anon enabled
        self.setup_clean_chain = True
        self.num_nodes = 2
        # We don't pass -anonrestricted param here and let the default value to be used which is true
        self.extra_args = [['-debug', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1', ] for i in range(self.num_nodes)]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self, split=False):
        self.add_nodes(self.num_nodes, extra_args=self.extra_args)
        self.start_nodes()

        self.connect_nodes_bi(0, 1) # Connect the two nodes
        self.sync_all()

    def run_test(self):
        nodes = self.nodes
        self.import_genesis_coins_a(nodes[0])
        self.import_genesis_coins_b(nodes[1])

        sx0 = nodes[0].getnewstealthaddress()

        # Create a transaction with anon output
        try:
            tx = nodes[1].sendtypeto('ghost', 'anon', [{'address': sx0, 'amount': 15}])
            assert(not self.wait_for_mempool(nodes[1], tx))
            self.stakeBlocks(2)
        except Exception as e:
            assert('Disabled output type.' in str(e))
        
        try:
            tx = nodes[1].sendtypeto('ghost', 'blind', [{'address': sx0, 'amount': 15}])
            assert(not self.wait_for_mempool(nodes[1], tx))
        except Exception as e:
            assert('Disabled output type.' in str(e))

        # Restart the nodes with anon tx enabled
        self.stop_node(1)
        self.stop_node(0)

        self.start_node(0, ['-wallet=default_wallet', '-debug', '-anonrestricted=0', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])
        self.start_node(1, ['-wallet=default_wallet', '-debug', '-anonrestricted=0', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])

        self.connect_nodes_bi(0, 1) # Connect the two nodes
        self.sync_all()

        tx = nodes[1].sendtypeto('ghost', 'anon', [{'address': sx0, 'amount': 15}])
        assert(self.wait_for_mempool(nodes[1], tx))
        assert(tx != "")
    
        tx = nodes[1].sendtypeto('ghost', 'blind', [{'address': sx0, 'amount': 15}])
        assert(self.wait_for_mempool(nodes[1], tx))
        assert(tx != "")

        # Start two nodes one accepting anon tx and another not. Then syncing should fail
        self.stop_node(1)
        self.stop_node(0)

        self.start_node(0, ['-wallet=default_wallet', '-debug', '-anonrestricted=0', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])
        self.start_node(1, ['-wallet=default_wallet', '-debug', '-reservebalance=10000000', '-stakethreadconddelayms=500', '-txindex=1', '-maxtxfee=1'])
       
        self.connect_nodes_bi(0, 1) # Connect the two nodes

        sx1 = nodes[0].getnewstealthaddress()
        tx = nodes[0].sendtypeto('ghost', 'anon', [{'address': sx1, 'amount': 15}])
        assert (self.wait_for_mempool(nodes[0], tx))
        assert (tx != "")

        rtx = nodes[0].getrawtransaction(tx)
        try:
            # Fail to add anon tx to mempool for node 1
            r = nodes[1].sendrawtransaction(rtx)
            assert (not self.wait_for_mempool(nodes[1], r))
        except Exception as e:
            assert ('anon-blind-tx-invalid' in str(e))

        r = self.stakeBlocks(1, 0, False) # Stake in order to include the previously created tx inside a block

        node0Block1 = nodes[0].getblock(nodes[0].getblockhash(1))
        node0Block1Hex = nodes[0].getblock(nodes[0].getblockhash(1), 0)
        
        assert(tx in node0Block1['tx'])
        print("CREATED TXID : " + tx)

        ro = nodes[0].listtransactions()
        for transaction in ro:
            if tx == transaction['txid']:
                assert(transaction['type'] == 'anon')

        res = nodes[1].submitblock( node0Block1Hex )
        assert (res == "duplicate-invalid")

if __name__ == '__main__':
    ControlAnonTest().main()
