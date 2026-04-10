// Copyright (c) 2016-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <chainparams.h>
#include <common/args.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
#include <util/chaintype.h>
#include <validation.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

// These are the two major time-sinks which happen after we have fully received
// a block off the wire, but before we can relay the block on to peers using
// compact block relay.

static CBlock MakeBenchmarkBlock(const CChainParams& params)
{
    CMutableTransaction coinbase_tx;
    coinbase_tx.vin.resize(1);
    coinbase_tx.vin[0].prevout.SetNull();
    coinbase_tx.vin[0].scriptSig = CScript{} << 1 << OP_0;
    coinbase_tx.vout.emplace_back(50 * COIN, CScript{} << OP_TRUE);

    CBlock block;
    block.vtx = {MakeTransactionRef(std::move(coinbase_tx))};
    block.nVersion = params.GenesisBlock().nVersion;
    block.hashPrevBlock = params.GenesisBlock().GetHash();
    block.hashMerkleRoot = BlockMerkleRoot(block);
    block.nTime = params.GenesisBlock().nTime + 1;
    block.nBits = params.GenesisBlock().nBits;

    const uint256 seed_hash{GetRandomXSeedHash(nullptr)};
    while (!CheckProofOfWork(GetBlockPoWHash(block, seed_hash), block.nBits, params.GetConsensus())) {
        ++block.nNonce;
    }

    return block;
}

static DataStream MakeBenchmarkBlockStream(const CChainParams& params)
{
    DataStream stream{};
    const CBlock block{MakeBenchmarkBlock(params)};
    stream << TX_WITH_WITNESS(block);
    return stream;
}

static void DeserializeBlockTest(benchmark::Bench& bench)
{
    ArgsManager bench_args;
    const auto chain_params{CreateChainParams(bench_args, ChainType::REGTEST)};

    DataStream stream{MakeBenchmarkBlockStream(*chain_params)};
    const size_t block_size{stream.size()};
    std::byte a{0};
    stream.write({&a, 1}); // Prevent compaction

    bench.unit("block").run([&] {
        CBlock block;
        stream >> TX_WITH_WITNESS(block);
        bool rewound = stream.Rewind(block_size);
        assert(rewound);
    });
}

static void DeserializeAndCheckBlockTest(benchmark::Bench& bench)
{
    ArgsManager bench_args;
    const auto chainParams = CreateChainParams(bench_args, ChainType::REGTEST);

    DataStream stream{MakeBenchmarkBlockStream(*chainParams)};
    const size_t block_size{stream.size()};
    std::byte a{0};
    stream.write({&a, 1}); // Prevent compaction

    bench.unit("block").run([&] {
        CBlock block; // Note that CBlock caches its checked state, so we need to recreate it here
        stream >> TX_WITH_WITNESS(block);
        bool rewound = stream.Rewind(block_size);
        assert(rewound);

        BlockValidationState validationState;
        bool checked = CheckBlock(block, validationState, chainParams->GetConsensus());
        assert(checked);
    });
}

BENCHMARK(DeserializeBlockTest, benchmark::PriorityLevel::HIGH);
BENCHMARK(DeserializeAndCheckBlockTest, benchmark::PriorityLevel::HIGH);
