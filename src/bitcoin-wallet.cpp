// Copyright (c) 2016-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <chainparams.h>
#include <chainparamsbase.h>
#include <logging.h>
#include <util/system.h>
#include <util/translation.h>
#include <util/url.h>
#include <wallet/wallettool.h>

#include <key/mnemonic.h>

#include <functional>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;
UrlDecodeFn* const URL_DECODE = nullptr;

static void SetupWalletToolArgs(ArgsManager& argsman)
{
    SetupHelpOptions(argsman);
    SetupChainParamsBaseOptions(argsman);

    argsman.AddArg("-datadir=<dir>", "Specify data directory", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-wallet=<wallet-name>", "Specify wallet name", ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::OPTIONS);
    argsman.AddArg("-debug=<category>", "Output debugging information (default: 0).", ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-printtoconsole", "Send trace/debug info to console (default: 1 when no -debug is true, 0 otherwise).", ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);

    argsman.AddArg("info", "Get wallet info", ArgsManager::ALLOW_ANY, OptionsCategory::COMMANDS);
    argsman.AddArg("create", "Create new wallet file", ArgsManager::ALLOW_ANY, OptionsCategory::COMMANDS);
    argsman.AddArg("salvage", "Attempt to recover private keys from a corrupt wallet. Warning: 'salvage' is experimental.", ArgsManager::ALLOW_ANY, OptionsCategory::COMMANDS);

    // Ghost
    argsman.AddArg("generatemnemonic", "Generate a new mnemonic: <language> <bytes_entropy>", ArgsManager::ALLOW_ANY, OptionsCategory::COMMANDS);
    argsman.AddArg("-btcmode", "", ArgsManager::ALLOW_ANY, OptionsCategory::HIDDEN);
}

static bool WalletAppInit(int argc, char* argv[])
{
    SetupWalletToolArgs(gArgs);
    std::string error_message;
    if (!gArgs.ParseParameters(argc, argv, error_message)) {
        tfm::format(std::cerr, "Error parsing command line arguments: %s\n", error_message);
        return false;
    }
    if (argc < 2 || HelpRequested(gArgs)) {
        std::string usage = strprintf("%s ghost-wallet version", PACKAGE_NAME) + " " + FormatFullVersion() + "\n\n" +
                                      "ghost-wallet is an offline tool for creating and interacting with " PACKAGE_NAME " wallet files.\n" +
                                      "By default ghost-wallet will act on wallets in the default mainnet wallet directory in the datadir.\n" +
                                      "To change the target wallet, use the -datadir, -wallet and -testnet/-regtest arguments.\n\n" +
                                      "Usage:\n" +
                                     "  ghost-wallet [options] <command>\n\n" +
                                     gArgs.GetHelpMessage();

        tfm::format(std::cout, "%s", usage);
        return false;
    }

    fParticlMode = !gArgs.GetBoolArg("-btcmode", false); // qa tests

    // check for printtoconsole, allow -debug
    LogInstance().m_print_to_console = gArgs.GetBoolArg("-printtoconsole", gArgs.GetBoolArg("-debug", false));

    if (!CheckDataDirOption()) {
        tfm::format(std::cerr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", ""));
        return false;
    }
    // Check for chain settings (Params() calls are only valid after this clause)
    SelectParams(gArgs.GetChainName());
    if (!fParticlMode) {
        WITNESS_SCALE_FACTOR = WITNESS_SCALE_FACTOR_BTC;
        if (gArgs.GetChainName() == CBaseChainParams::REGTEST) {
            ResetParams(CBaseChainParams::REGTEST, fParticlMode);
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();
    RandomInit();


    bool show_help = false;
    for (int i = 1; i < argc; ++i) {
        if (IsSwitchChar(argv[i][0])) {
            char *p = argv[i];
            while (*p == '-') p++;
            if (strcmp(p, "?") == 0 || strcmp(p, "h") == 0 || strcmp(p, "help") == 0) {
                show_help = true;
            }
            continue;
        }
        if (strcmp(argv[i], "generatemnemonic") == 0) {
            if (show_help) {
                std::string usage = "generatemnemonic <language> <bytes_entropy>\n"
                    "\nArguments:\n"
                    "1. language        (string, optional, default=english) Which wordlist to use (" + mnemonic::ListEnabledLanguages(", ") + ").\n"
                    "2. bytes_entropy   (numeric, optional, default=32) Affects length of mnemonic, [16, 64].\n";
                tfm::format(std::cout, "%s\n", usage);
                return EXIT_SUCCESS;
            }

            int nLanguage = mnemonic::WLL_ENGLISH;
            int nBytesEntropy = 32;

            if (argc > i + 1) {
                nLanguage = mnemonic::GetLanguageOffset(argv[i+1]);
            }
            if (argc > i + 2) {
                if (!ParseInt32(argv[i+2], &nBytesEntropy)) {
                    tfm::format(std::cerr, "Error: Invalid num bytes entropy.\n");
                    return EXIT_FAILURE;
                }
                if (nBytesEntropy < 16 || nBytesEntropy > 64) {
                    tfm::format(std::cerr, "Error: Num bytes entropy out of range [16,64].\n");
                    return EXIT_FAILURE;
                }
            }
            std::string sMnemonic, sError;
            std::vector<uint8_t> vEntropy(nBytesEntropy);

            GetStrongRandBytes2(&vEntropy[0], nBytesEntropy);
            if (0 != mnemonic::Encode(nLanguage, vEntropy, sMnemonic, sError)) {
                tfm::format(std::cerr, "Error: MnemonicEncode failed %s.\n", sError);
                return EXIT_FAILURE;
            }

            tfm::format(std::cout, "%s\n", sMnemonic);
            return EXIT_SUCCESS;
        }
    }


    try {
        if (!WalletAppInit(argc, argv)) return EXIT_FAILURE;
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, "WalletAppInit()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(nullptr, "WalletAppInit()");
        return EXIT_FAILURE;
    }

    std::string method {};
    for(int i = 1; i < argc; ++i) {
        if (!IsSwitchChar(argv[i][0])) {
            if (!method.empty()) {
                tfm::format(std::cerr, "Error: two methods provided (%s and %s). Only one method should be provided.\n", method, argv[i]);
                return EXIT_FAILURE;
            }
            method = argv[i];
        }
    }

    if (method.empty()) {
        tfm::format(std::cerr, "No method provided. Run `ghost-wallet -help` for valid methods.\n");
        return EXIT_FAILURE;
    }

    // A name must be provided when creating a file
    if (method == "create" && !gArgs.IsArgSet("-wallet")) {
        tfm::format(std::cerr, "Wallet name must be provided when creating a new wallet.\n");
        return EXIT_FAILURE;
    }

    std::string name = gArgs.GetArg("-wallet", "");

    ECCVerifyHandle globalVerifyHandle;
    ECC_Start();
    if (!WalletTool::ExecuteWalletToolFunc(method, name))
        return EXIT_FAILURE;
    ECC_Stop();
    return EXIT_SUCCESS;
}
