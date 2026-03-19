// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <crypto/hex_base.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <span>
#include <utility>

using namespace util::hex_literals;

// Workaround MSVC bug triggering C7595 when calling consteval constructors in
// initializer lists.
// https://developercommunity.visualstudio.com/t/Bogus-C7595-error-on-valid-C20-code/10906093
#if defined(_MSC_VER)
auto consteval_ctor(auto&& input) { return input; }
#else
#define consteval_ctor(input) (input)
#endif

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the RNG genesis block.
 *
 * Key differences from Bitcoin:
 * - Coinbase message: "Life is a random number generator"
 * - Output is OP_RETURN (provably unspendable, not just by convention)
 * - Reward is still 50 RNG but cannot be spent
 * - Version is 0x20000000 (BIP9 enabled from genesis)
 *
 * The OP_RETURN output contains a commitment to the genesis identity,
 * making the genesis reward provably unspendable while preserving
 * the 21M supply calculations (genesis 50 RNG is effectively burned).
 */
static CBlock CreateRNGGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    // RNG genesis message
    const char* pszTimestamp = "Life is a random number generator";

    // OP_RETURN makes output provably unspendable (unlike Bitcoin's OP_CHECKSIG)
    // Identity commitment: "RNG Genesis"
    const CScript genesisOutputScript = CScript() << OP_RETURN << "524e472047656e65736973"_hex;

    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 2100000; // RNG: 2.1M-block halving interval
        // RNG: No script flag exceptions - clean chain from genesis
        consensus.BIP34Height = 0; // RNG: All BIPs active from genesis
        consensus.BIP34Hash = uint256{}; // Will be set to genesis hash
        consensus.BIP65Height = 0; // RNG: Active from genesis
        consensus.BIP66Height = 0; // RNG: Active from genesis
        consensus.CSVHeight = 0; // RNG: Active from genesis
        consensus.SegwitHeight = 0; // RNG: Active from genesis
        consensus.MinBIP9WarningHeight = 0;
        // RNG: powLimit must match genesis nBits (0x207fffff)
        consensus.powLimit = uint256{"7fffff0000000000000000000000000000000000000000000000000000000000"};
        consensus.nPowTargetTimespan = 120; // RNG: kept for compatibility but LWMA used
        consensus.nPowTargetSpacing = 120; // RNG: 2-minute blocks (Monero-style)
        consensus.nDifficultyWindow = 720; // Monero-style: 720 block window
        consensus.nDifficultyCut = 60;     // Monero-style: cut 60 outlier timestamps from each end
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // RNG: Taproot always active from genesis (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // Active from genesis
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        consensus.nMinimumChainWork = uint256{}; // RNG: New chain, no minimum work yet
        consensus.defaultAssumeValid = uint256{}; // RNG: New chain, no assumed valid block yet

        /**
         * RNG network magic bytes: 0xB07C010E
         * Spells "RNG" with checksum - distinguishes from Bitcoin network.
         */
        pchMessageStart[0] = 0xb0;
        pchMessageStart[1] = 0x7c;
        pchMessageStart[2] = 0x01;
        pchMessageStart[3] = 0x0e;
        nDefaultPort = 8433; // RNG: P2P port
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 810;
        m_assumed_chain_state_size = 14;

        // RNG genesis block with the canonical RNG message
        // nTime: 1738195200 = 2025-01-30 00:00:00 UTC (launch preparation)
        // nBits: 0x207fffff = easiest safe difficulty (calibrated for 60s blocks with 1 miner at ~1kH/s (launch phase))
        // nVersion: 0x20000000 = BIP9 enabled from genesis
        genesis = CreateRNGGenesisBlock(1738195200, 0, 0x207fffff, 0x20000000, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;
        assert(consensus.hashGenesisBlock == uint256{"83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4"});
        assert(genesis.hashMerkleRoot == uint256{"b713a92ad8104e5a1650d02f96df9cb18bd6a39a222829ba4e4b5e79e4de7232"});

        // Public IPv4 seeds currently serving the live chain.
        vSeeds.emplace_back("95.111.239.142");
        vSeeds.emplace_back("161.97.114.192");
        vSeeds.emplace_back("185.218.126.23");
        vSeeds.emplace_back("185.239.209.227");

        // RNG address prefixes
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,25); // RNG: 'B' prefix for P2PKH
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);  // 'A' prefix for P2SH (same as Bitcoin)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E}; // bpub
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4}; // bprv

        bech32_hrp = "rng"; // RNG: bech32 addresses start with rng1

        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // Supported fast-bootstrap snapshot exported from the live chain on
        // 2026-03-19. Nodes can load bootstrap/rng-mainnet-15091.utxo with
        // loadtxoutset and continue syncing from this base height.
        m_assumeutxo_data = {
            AssumeutxoData{
                .height = 15091,
                .hash_serialized = AssumeutxoHash{uint256{"9ca1b551b9837c0b0e9158436bac5051e4984d39f691e1374c4786a6c0ed5393"}},
                .m_chain_tx_count = 15107,
                .blockhash = uint256{"2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb"},
            },
        };

        chainTxData = ChainTxData{
            .nTime    = 1773948965,
            .tx_count = 15107,
            .dTxRate  = 0.0084,
        };

        // RNG: Default header sync params
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 632,
            .redownload_buffer_size = 15009,
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 2100000; // RNG: Same as mainnet
        // RNG: No script flag exceptions - clean chain from genesis
        consensus.BIP34Height = 0; // RNG: All BIPs active from genesis
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 0;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        // RNG: powLimit = 0x00000377ae... is the easiest safe target (won't overflow)
        consensus.powLimit = uint256{"7fffff0000000000000000000000000000000000000000000000000000000000"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60; // RNG: 60 second blocks
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // RNG: Taproot always active from genesis (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        consensus.nMinimumChainWork = uint256{}; // RNG: New chain
        consensus.defaultAssumeValid = uint256{}; // RNG: New chain

        // RNG testnet network magic: 0xB07C7E57 (RNG TEST)
        pchMessageStart[0] = 0xb0;
        pchMessageStart[1] = 0x7c;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0x57;
        nDefaultPort = 18433; // RNG testnet P2P port
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 240;
        m_assumed_chain_state_size = 19;

        // RNG testnet genesis - same message, different nonce
        genesis = CreateRNGGenesisBlock(1738195200, 1, 0x207fffff, 0x20000000, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // TODO: Assertions updated after genesis mining

        vFixedSeeds.clear();
        vSeeds.clear();
        // RNG testnet seeds (to be configured at launch)
        vSeeds.emplace_back("testnet-seed1.rng.network.");
        vSeeds.emplace_back("testnet-seed2.rng.network.");

        // RNG testnet uses same prefixes as Bitcoin testnet for familiarity
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111); // 't' prefix
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // 's' prefix
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239); // 'c' prefix
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF}; // tpub
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94}; // tprv

        bech32_hrp = "trng"; // RNG testnet: trng1...

        vFixedSeeds.clear(); // RNG: No fixed seeds yet

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // RNG testnet: No assumeutxo data yet
        m_assumeutxo_data = {};

        // RNG testnet: New chain, no transaction data yet
        chainTxData = ChainTxData{
            .nTime    = 0,
            .tx_count = 0,
            .dTxRate  = 0.0,
        };

        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 628,
            .redownload_buffer_size = 13460,
        };
    }
};

/**
 * Testnet (v4): public test network which is reset from time to time.
 */
class CTestNet4Params : public CChainParams {
public:
    CTestNet4Params() {
        m_chain_type = ChainType::TESTNET4;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 2100000; // RNG: Same as mainnet
        consensus.BIP34Height = 0; // RNG: All BIPs active from genesis
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 0;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        // RNG: powLimit = 0x00000377ae... is the easiest safe target (won't overflow)
        consensus.powLimit = uint256{"7fffff0000000000000000000000000000000000000000000000000000000000"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60; // RNG: 60 second blocks
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = true;
        consensus.fPowNoRetargeting = false;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        consensus.nMinimumChainWork = uint256{}; // RNG: New chain
        consensus.defaultAssumeValid = uint256{}; // RNG: New chain

        // RNG testnet4 network magic
        pchMessageStart[0] = 0xb0;
        pchMessageStart[1] = 0x7c;
        pchMessageStart[2] = 0x74;  // 't' for testnet4
        pchMessageStart[3] = 0x34;  // '4' for testnet4
        nDefaultPort = 48433; // RNG testnet4 P2P port
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 22;
        m_assumed_chain_state_size = 2;

        // RNG testnet4 genesis - same RNG message
        genesis = CreateRNGGenesisBlock(1738195200, 2, 0x207fffff, 0x20000000, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // TODO: Assertions updated after genesis mining

        vFixedSeeds.clear();
        vSeeds.clear();
        // RNG testnet4 seeds (to be configured at launch)
        vSeeds.emplace_back("testnet4-seed1.rng.network.");
        vSeeds.emplace_back("testnet4-seed2.rng.network.");

        // RNG testnet4 uses same testnet prefixes
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111); // 't' prefix
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // 's' prefix
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239); // 'c' prefix
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF}; // tpub
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94}; // tprv

        bech32_hrp = "trng"; // RNG testnet4: trng1...

        vFixedSeeds.clear(); // RNG: No fixed seeds yet

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // RNG testnet4: No assumeutxo data yet
        m_assumeutxo_data = {};

        // RNG testnet4: New chain, no transaction data yet
        chainTxData = ChainTxData{
            .nTime    = 0,
            .tx_count = 0,
            .dTxRate  = 0.0,
        };

        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 275,
            .redownload_buffer_size = 7017,
        };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vFixedSeeds.clear();
        vSeeds.clear();

        if (!options.challenge) {
            bin = "512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae"_hex_v_u8;
            vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_signet), std::end(chainparams_seed_signet));
            vSeeds.emplace_back("seed.signet.bitcoin.sprovoost.nl.");
            vSeeds.emplace_back("seed.signet.achownodes.xyz."); // Ava Chow, only supports x1, x5, x9, x49, x809, x849, xd, x400, x404, x408, x448, xc08, xc48, x40c

            consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000000000000067d328e681a"};
            consensus.defaultAssumeValid = uint256{"000000128586e26813922680309f04e1de713c7542fee86ed908f56368aefe2e"}; // 267665
            m_assumed_blockchain_size = 20;
            m_assumed_chain_state_size = 4;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 000000128586e26813922680309f04e1de713c7542fee86ed908f56368aefe2e
                .nTime    = 1756723017,
                .tx_count = 26185472,
                .dTxRate  = 0.7452721495389969,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogInfo("Signet with challenge %s", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 2100000; // RNG: Same as mainnet
        consensus.BIP34Height = 0; // RNG: All BIPs active from genesis
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 0;
        consensus.SegwitHeight = 0;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60; // RNG: 60 second blocks
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"7fffff0000000000000000000000000000000000000000000000000000000000"};
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 38433; // RNG signet P2P port
        nPruneAfterHeight = 1000;

        // RNG signet genesis - same RNG message
        genesis = CreateRNGGenesisBlock(1738195200, 3, 0x207fffff, 0x20000000, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // TODO: Assertions updated after genesis mining

        // RNG signet: No assumeutxo data yet
        m_assumeutxo_data = {};

        // RNG signet uses same testnet prefixes
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111); // 't' prefix
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // 's' prefix
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239); // 'c' prefix
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF}; // tpub
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94}; // tprv

        bech32_hrp = "trng"; // RNG signet: trng1...

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // Generated by headerssync-params.py on 2025-09-03.
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 390,
            .redownload_buffer_size = 9584, // 9584/390 = ~24.6 commitments
        };
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150; // Keep short for regtest
        consensus.BIP34Height = 0; // RNG: All BIPs active from genesis
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 0;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one day
        consensus.nPowTargetSpacing = 60; // RNG: 60 second blocks
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = opts.enforce_bip94;
        consensus.fPowNoRetargeting = true;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 144;

        // RNG: Taproot always active from genesis
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 144;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        // RNG regtest network magic: 0xB07C0000
        pchMessageStart[0] = 0xb0;
        pchMessageStart[1] = 0x7c;
        pchMessageStart[2] = 0x00;
        pchMessageStart[3] = 0x00;
        nDefaultPort = 18544; // RNG regtest P2P port
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        // RNG regtest genesis - minimal difficulty for instant mining
        // nBits: 0x207fffff = very easy target for testing
        // nNonce: 1 = valid RandomX nonce producing hash below target
        genesis = CreateRNGGenesisBlock(1738195200, 1, 0x207fffff, 0x20000000, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        // RNG: Clear assumeutxo data - new chain with different genesis
        // TODO: Regenerate assumeutxo data after chain stabilizes
        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            .nTime = 0,
            .tx_count = 0,
            .dTxRate = 0.001, // Set a non-zero rate to make it testable
        };

        // RNG regtest uses same testnet prefixes
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111); // 't' prefix
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // 's' prefix
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239); // 'c' prefix
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF}; // tpub
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94}; // tprv

        bech32_hrp = "trng"; // RNG regtest: trng1...

        // Copied from Testnet4.
        m_headers_sync_params = HeadersSyncParams{
            .commitment_period = 275,
            .redownload_buffer_size = 7017, // 7017/275 = ~25.5 commitments
        };
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet4()
{
    return std::make_unique<const CTestNet4Params>();
}

std::vector<int> CChainParams::GetAvailableSnapshotHeights() const
{
    std::vector<int> heights;
    heights.reserve(m_assumeutxo_data.size());

    for (const auto& data : m_assumeutxo_data) {
        heights.emplace_back(data.height);
    }
    return heights;
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    const auto testnet_msg = CChainParams::TestNet()->MessageStart();
    const auto testnet4_msg = CChainParams::TestNet4()->MessageStart();
    const auto regtest_msg = CChainParams::RegTest({})->MessageStart();
    const auto signet_msg = CChainParams::SigNet({})->MessageStart();

    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    } else if (std::ranges::equal(message, testnet_msg)) {
        return ChainType::TESTNET;
    } else if (std::ranges::equal(message, testnet4_msg)) {
        return ChainType::TESTNET4;
    } else if (std::ranges::equal(message, regtest_msg)) {
        return ChainType::REGTEST;
    } else if (std::ranges::equal(message, signet_msg)) {
        return ChainType::SIGNET;
    }
    return std::nullopt;
}
