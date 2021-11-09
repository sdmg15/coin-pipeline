// Copyright (c) 2017-2020 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/hdwallet_test_fixture.h>
#include <bench/bench.h>
#include <wallet/hdwallet.h>
#include <wallet/coincontrol.h>
#include <interfaces/chain.h>

#include <validation.h>
#include <blind.h>
#include <rpc/rpcutil.h>
#include <rpc/blockchain.h>
#include <timedata.h>
#include <miner.h>
#include <pos/miner.h>
#include <util/string.h>
#include <util/translation.h>

CTransactionRef CreateTxn(CHDWallet *pwallet, CBitcoinAddress &address, CAmount amount, int type_in, int type_out, int nRingSize = 5)
{
    gArgs.ForceSetArg("-anonrestricted", "0");
    LOCK(pwallet->cs_wallet);

    assert(address.IsValid());

    std::vector<CTempRecipient> vecSend;
    std::string sError;
    CTempRecipient r;
    r.nType = type_out;
    r.SetAmount(amount);
    r.address = address.Get();
    vecSend.push_back(r);

    CTransactionRef tx_new;
    CWalletTx wtx(pwallet, tx_new);
    CTransactionRecord rtx;
    CAmount nFee;
    CCoinControl coinControl;
    if (type_in == OUTPUT_STANDARD) {
        assert(0 == pwallet->AddStandardInputs(wtx, rtx, vecSend, true, nFee, &coinControl, sError));
    } else
    if (type_in == OUTPUT_CT) {
        assert(0 == pwallet->AddBlindedInputs(wtx, rtx, vecSend, true, nFee, &coinControl, sError));
    } else {
        int nInputsPerSig = 1;
        assert(0 == pwallet->AddAnonInputs(wtx, rtx, vecSend, true, nRingSize, nInputsPerSig, nFee, &coinControl, sError));
    }
    return wtx.tx;
}

static void AddAnonTxn(CHDWallet *pwallet, CBitcoinAddress &address, CAmount amount, OutputTypes output_type)
{
    {
    gArgs.ForceSetArg("-anonrestricted", "0");
    LOCK(pwallet->cs_wallet);

    assert(address.IsValid());

    std::vector<CTempRecipient> vecSend;
    std::string sError;
    CTempRecipient r;
    r.nType = output_type;
    r.SetAmount(amount);
    r.address = address.Get();
    vecSend.push_back(r);

    CTransactionRef tx_new;
    CWalletTx wtx(pwallet, tx_new);
    CTransactionRecord rtx;
    CAmount nFee;
    CCoinControl coinControl;
    assert(0 == pwallet->AddStandardInputs(wtx, rtx, vecSend, true, nFee, &coinControl, sError));
    assert(wtx.SubmitMemoryPoolAndRelay(sError, true));
    }
    SyncWithValidationInterfaceQueue();
}

void StakeNBlocks(CHDWallet *pwallet, size_t nBlocks)
{
    int nBestHeight;
    size_t nStaked = 0;
    size_t k, nTries = 10000;
    for (k = 0; k < nTries; ++k) {
        nBestHeight = pwallet->chain().getHeightInt();

        int64_t nSearchTime = GetAdjustedTime() & ~Params().GetStakeTimestampMask(nBestHeight+1);
        if (nSearchTime <= pwallet->nLastCoinStakeSearchTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }

        std::unique_ptr<CBlockTemplate> pblocktemplate = pwallet->CreateNewBlock();
        assert(pblocktemplate.get());

        if (pwallet->SignBlock(pblocktemplate.get(), nBestHeight+1, nSearchTime)) {
            CBlock *pblock = &pblocktemplate->block;

            if (CheckStake(pblock)) {
                nStaked++;
            }
        }

        if (nStaked >= nBlocks) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    assert(k < nTries);
    SyncWithValidationInterfaceQueue();
};

static std::shared_ptr<CHDWallet> CreateTestWallet(interfaces::Chain& chain, std::string wallet_name)
{
    DatabaseOptions options;
    DatabaseStatus status;
    bilingual_str error;
    std::vector<bilingual_str> warnings;
    options.create_flags = WALLET_FLAG_BLANK_WALLET;
    auto database = MakeWalletDatabase(wallet_name, options, status, error);
    auto wallet = CWallet::Create(chain, wallet_name, std::move(database), options.create_flags, error, warnings);

    return std::static_pointer_cast<CHDWallet>(wallet);
}

static void AddTx(benchmark::Bench& bench, const std::string from, const std::string to, const bool owned)
{
    gArgs.ForceSetArg("-acceptanontxn", "1"); // TODO: remove
    gArgs.ForceSetArg("-acceptblindtxn", "1"); // TODO: remove
    gArgs.ForceSetArg("-anonrestricted", "0");

    TestingSetup test_setup{CBaseChainParams::REGTEST, {}, true};
    util::Ref context{test_setup.m_node};

    ECC_Start_Stealth();
    ECC_Start_Blinding();

    std::unique_ptr<interfaces::Chain> m_chain = interfaces::MakeChain(test_setup.m_node);
    std::unique_ptr<interfaces::ChainClient> m_chain_client = interfaces::MakeWalletClient(*m_chain, *Assert(test_setup.m_node.args));
    m_chain_client->registerRpcs();

    std::shared_ptr<CHDWallet> pwallet_a = CreateTestWallet(*m_chain.get(), "a");
    assert(pwallet_a.get());
    AddWallet(pwallet_a);

    std::shared_ptr<CHDWallet> pwallet_b = CreateTestWallet(*m_chain.get(), "b");
    assert(pwallet_b.get());
    AddWallet(pwallet_b);

    {
        int last_height = ::ChainActive().Height();
        uint256 last_hash = ::ChainActive().Tip()->GetBlockHash();
        {
            LOCK(pwallet_a->cs_wallet);
            pwallet_a->SetLastBlockProcessed(last_height, last_hash);
        }
        {
            LOCK(pwallet_b->cs_wallet);
            pwallet_b->SetLastBlockProcessed(last_height, last_hash);
        }
    }

    std::string from_address_type, to_address_type;
    OutputTypes from_tx_type = OUTPUT_NULL;
    OutputTypes to_tx_type = OUTPUT_NULL;

    UniValue rv;

    CallRPC("extkeyimportmaster tprv8ZgxMBicQKsPeK5mCpvMsd1cwyT1JZsrBN82XkoYuZY1EVK7EwDaiL9sDfqUU5SntTfbRfnRedFWjg5xkDG5i3iwd3yP7neX5F2dtdCojk4", context, "a");
    CallRPC("extkeyimportmaster \"expect trouble pause odor utility palace ignore arena disorder frog helmet addict\"", context, "b");

    if (from == "plain") {
        from_address_type = "getnewaddress";
        from_tx_type = OUTPUT_STANDARD;
    } else if (from == "blind") {
        from_address_type = "getnewstealthaddress";
        from_tx_type = OUTPUT_CT;
    } else if (from == "anon") {
        from_address_type = "getnewstealthaddress";
        from_tx_type = OUTPUT_RINGCT;
    }

    if (to == "plain") {
        to_address_type = "getnewaddress";
        to_tx_type = OUTPUT_STANDARD;
    } else if (to == "blind") {
        to_address_type = "getnewstealthaddress";
        to_tx_type = OUTPUT_CT;
    } else if (to == "anon") {
        to_address_type = "getnewstealthaddress";
        to_tx_type = OUTPUT_RINGCT;
    }

    assert(from_tx_type != OUTPUT_NULL);
    assert(to_tx_type != OUTPUT_NULL);

    rv = CallRPC(from_address_type, context, "a");
    CBitcoinAddress addr_a(part::StripQuotes(rv.write()));

    rv = CallRPC(to_address_type, context, "b");
    CBitcoinAddress addr_b(part::StripQuotes(rv.write()));

    if (from == "anon" || from == "blind") {
        AddAnonTxn(pwallet_a.get(), addr_a, 1 * COIN, from == "anon" ? OUTPUT_RINGCT : OUTPUT_CT);
        AddAnonTxn(pwallet_a.get(), addr_a, 1 * COIN, from == "anon" ? OUTPUT_RINGCT : OUTPUT_CT);
        AddAnonTxn(pwallet_a.get(), addr_a, 1 * COIN, from == "anon" ? OUTPUT_RINGCT : OUTPUT_CT);
        AddAnonTxn(pwallet_a.get(), addr_a, 1 * COIN, from == "anon" ? OUTPUT_RINGCT : OUTPUT_CT);
        AddAnonTxn(pwallet_a.get(), addr_a, 1 * COIN, from == "anon" ? OUTPUT_RINGCT : OUTPUT_CT);

        StakeNBlocks(pwallet_a.get(), 2);
    }

    CTransactionRef tx = CreateTxn(pwallet_a.get(), owned ? addr_b : addr_a, 1000, from_tx_type, to_tx_type);

    CWalletTx::Confirmation confirm;
    bench.run([&] {
        LOCK(pwallet_b.get()->cs_wallet);
        pwallet_b.get()->AddToWalletIfInvolvingMe(tx, confirm, true);
    });

    RemoveWallet(pwallet_a, nullopt);
    pwallet_a.reset();

    RemoveWallet(pwallet_b, nullopt);
    pwallet_b.reset();

    ECC_Stop_Stealth();
    ECC_Stop_Blinding();
}

static void ParticlAddTxPlainPlainNotOwned(benchmark::Bench& bench) { AddTx(bench, "plain", "plain", false); }
static void ParticlAddTxPlainPlainOwned(benchmark::Bench& bench) { AddTx(bench, "plain", "plain", true); }
static void ParticlAddTxPlainBlindNotOwned(benchmark::Bench& bench) { AddTx(bench, "plain", "blind", false); }
static void ParticlAddTxPlainBlindOwned(benchmark::Bench& bench) { AddTx(bench, "plain", "blind", true); }
// static void ParticlAddTxPlainAnonNotOwned(benchmark::Bench& bench) { AddTx(bench, "plain", "anon", false); }
// static void ParticlAddTxPlainAnonOwned(benchmark::Bench& bench) { AddTx(bench, "plain", "anon", true); }

static void ParticlAddTxBlindPlainNotOwned(benchmark::Bench& bench) { AddTx(bench, "blind", "plain", false); }
static void ParticlAddTxBlindPlainOwned(benchmark::Bench& bench) { AddTx(bench, "blind", "plain", true); }
static void ParticlAddTxBlindBlindNotOwned(benchmark::Bench& bench) { AddTx(bench, "blind", "blind", false); }
static void ParticlAddTxBlindBlindOwned(benchmark::Bench& bench) { AddTx(bench, "blind", "blind", true); }
static void ParticlAddTxBlindAnonNotOwned(benchmark::Bench& bench) { AddTx(bench, "blind", "anon", false); }
static void ParticlAddTxBlindAnonOwned(benchmark::Bench& bench) { AddTx(bench, "blind", "anon", true); }

static void ParticlAddTxAnonPlainNotOwned(benchmark::Bench& bench) { AddTx(bench, "anon", "plain", false); }
static void ParticlAddTxAnonPlainOwned(benchmark::Bench& bench) { AddTx(bench, "anon", "plain", true); }
static void ParticlAddTxAnonBlindNotOwned(benchmark::Bench& bench) { AddTx(bench, "anon", "blind", false); }
static void ParticlAddTxAnonBlindOwned(benchmark::Bench& bench) { AddTx(bench, "anon", "blind", true); }
static void ParticlAddTxAnonAnonNotOwned(benchmark::Bench& bench) { AddTx(bench, "anon", "anon", false); }
static void ParticlAddTxAnonAnonOwned(benchmark::Bench& bench) { AddTx(bench, "anon", "anon", true); }

BENCHMARK(ParticlAddTxPlainPlainNotOwned);
BENCHMARK(ParticlAddTxPlainPlainOwned);
BENCHMARK(ParticlAddTxPlainBlindNotOwned);
BENCHMARK(ParticlAddTxPlainBlindOwned);
// BENCHMARK(ParticlAddTxPlainAnonNotOwned);
// BENCHMARK(ParticlAddTxPlainAnonOwned);

BENCHMARK(ParticlAddTxBlindPlainNotOwned);
BENCHMARK(ParticlAddTxBlindPlainOwned);
BENCHMARK(ParticlAddTxBlindBlindNotOwned);
BENCHMARK(ParticlAddTxBlindBlindOwned);
BENCHMARK(ParticlAddTxBlindAnonNotOwned);
BENCHMARK(ParticlAddTxBlindAnonOwned);

BENCHMARK(ParticlAddTxAnonPlainNotOwned);
BENCHMARK(ParticlAddTxAnonPlainOwned);
BENCHMARK(ParticlAddTxAnonBlindNotOwned);
BENCHMARK(ParticlAddTxAnonBlindOwned);
BENCHMARK(ParticlAddTxAnonAnonNotOwned);
BENCHMARK(ParticlAddTxAnonAnonOwned);
