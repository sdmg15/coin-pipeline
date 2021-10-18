// Copyright (c) 2021 barrystyle
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <adapter.h>

bool is_ghost_debug() {
    return gArgs.GetBoolArg("-ghostdebug", DEFAULT_GHOSTDEBUG);
}

bool exploit_fixtime_passed(uint32_t nTime)
{
    uint32_t testTime = Params().GetConsensus().exploit_fix_2_time;
    if (nTime > testTime) {
        if (is_ghost_debug())
            LogPrintf("%s - returning true\n", __func__);
        return true;
    }
    if (is_ghost_debug())
        LogPrintf("%s - returning false\n", __func__);
    return false;
}

bool is_output_recovery_address(const std::string& dest) {
    const std::string recoveryAddress = Params().GetRecoveryAddress();
    if (dest.find(recoveryAddress) != std::string::npos) {
        return true;
    }
    return false;
}

bool is_anonblind_transaction_ok(const CTransactionRef& tx, unsigned int totalRing)
{
    //! enabled by default
    bool allowedForUse = true;

    if (totalRing > 0) {

        if (!Params().IsAnonEnabled()) {
            return false;
        }

        const uint256& txHash = tx->GetHash();
        if (txHash == TEST_TX) {
            return true;
        }

        //! for restricted anon/blind spends
        //! no mixed component stakes allowed
        if (tx->IsCoinStake()) {
            allowedForUse = false;
            LogPrintf("%s - transaction %s is a coinstake with anon/blind components\n", __func__, txHash.ToString());
        }

        //! total value out must be greater than 5 coins
        CAmount totalValue = tx->GetValueOut();
        if ((totalValue - (5 * COIN)) < 0) {
            allowedForUse = false;
            LogPrintf("%s - transaction %s has output of less than 5 coins total\n", __func__, txHash.ToString());
        }

        //! split among no more than three outputs
        unsigned int outSize = tx->vpout.size();
        if (outSize > Params().GetAnonMaxOutputSize()) {
            allowedForUse = false;
            LogPrintf("%s - transaction %s has more than 3 outputs total\n", __func__, txHash.ToString());
        }

        //! if allowedForUse is false by this stage
        if (!allowedForUse) {
            LogPrintf("%s - transaction %s failed early in tests\n", __func__, txHash.ToString());
            return false;
        }

        //! recovery address must receive 99.95% of the output amount
        allowedForUse = false;
        for (unsigned int i=0; i < outSize; ++i) {
            if (tx->vpout[i]->IsStandardOutput()) {
                std::string destTest = HexStr(tx->vpout[i]->GetStandardOutput()->scriptPubKey);
                if (is_output_recovery_address(destTest)) {
                    if (tx->vpout[i]->GetStandardOutput()->nValue >= totalValue * 0.995) {
                        LogPrintf("Found recovery amount at vout.n #%d\n", i);
                        allowedForUse = true;
                    }
                }
            }
        }
    }
    
    if(Params().IsAnonEnabled()){
        allowedForUse = true;
    }
    
    return allowedForUse;
}
