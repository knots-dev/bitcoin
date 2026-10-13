// Copyright (c) 2026 The Bitcoin Knots developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/cpuminer.h>

#include <consensus/merkle.h>
#include <interfaces/mining.h>
#include <logging.h>
#include <pow.h>
#include <primitives/block.h>
#include <util/check.h>
#include <util/strencodings.h>
#include <util/thread.h>
#include <util/signalinterrupt.h>
#include <validation.h>

#include <algorithm>
#include <limits>

namespace node {

namespace {
//! How many nonces to test between checks for a new chain tip / shutdown.
constexpr uint32_t NONCES_PER_CHECK{1U << 16};
//! Rebuild the template at least this often so nTime and mempool contents stay fresh.
constexpr int64_t TEMPLATE_MAX_AGE_SECS{60};

int64_t SteadySeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

int ResolveCpuMinerThreads(int requested)
{
    if (requested < 0) {
        return std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    }
    // 0 is treated like the original setgenerate semantics of "use the default".
    if (requested == 0) return DEFAULT_GENPROCLIMIT;
    return requested;
}

CpuMiner::CpuMiner(ChainstateManager& chainman, interfaces::Mining& mining)
    : m_chainman{chainman}, m_mining{mining}
{
}

CpuMiner::~CpuMiner()
{
    Stop();
}

void CpuMiner::Start(int threads, const CScript& coinbase_script)
{
    Stop();

    const int count{ResolveCpuMinerThreads(threads)};

    LOCK(m_mutex);
    m_stop = false;
    m_hashes = 0;
    m_start_time = SteadySeconds();
    m_thread_count = count;
    m_running = true;

    if (!m_mining.isTestChain()) {
        LogWarning("Built-in CPU miner started on a non-test chain with %d thread(s). "
                   "The expected time to find a block is effectively infinite; "
                   "use getblocktemplate with an external miner or the DATUM Gateway (contrib/datum) instead.", count);
    } else {
        LogInfo("Built-in CPU miner started with %d thread(s)", count);
    }

    m_workers.reserve(count);
    for (int i = 0; i < count; ++i) {
        m_workers.emplace_back([this, i, count, coinbase_script] { Worker(i, count, coinbase_script); });
    }
}

void CpuMiner::Stop()
{
    std::vector<std::thread> workers;
    {
        LOCK(m_mutex);
        m_stop = true;
        workers.swap(m_workers);
    }
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
    if (m_running.exchange(false)) {
        LogInfo("Built-in CPU miner stopped");
    }
}

double CpuMiner::HashesPerSec() const
{
    if (!m_running) return 0.0;
    const int64_t elapsed{SteadySeconds() - m_start_time.load()};
    if (elapsed <= 0) return 0.0;
    return static_cast<double>(m_hashes.load()) / static_cast<double>(elapsed);
}

void CpuMiner::Worker(int index, int total, CScript coinbase_script)
{
    util::ThreadRename(strprintf("cpuminer.%d", index));

    // Partition the 32-bit nonce space so threads never duplicate work on the
    // same template.
    const uint64_t slice{(uint64_t{std::numeric_limits<uint32_t>::max()} + 1) / static_cast<uint64_t>(total)};
    const uint64_t slice_begin{slice * static_cast<uint64_t>(index)};
    const uint64_t slice_end{index + 1 == total ? uint64_t{std::numeric_limits<uint32_t>::max()} + 1 : slice_begin + slice};

    const auto should_stop = [&] { return m_stop.load() || static_cast<bool>(m_chainman.m_interrupt); };
    const auto idle = [&](std::chrono::milliseconds d) {
        const auto deadline = std::chrono::steady_clock::now() + d;
        while (!should_stop() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    while (!should_stop()) {
        // Never mine on top of a chain we are still catching up to.
        if (!m_mining.isTestChain() && m_mining.isInitialBlockDownload()) {
            idle(std::chrono::seconds(1));
            continue;
        }

        const auto tip{m_mining.getTip()};
        if (!tip) {
            idle(std::chrono::seconds(1));
            continue;
        }

        std::unique_ptr<interfaces::BlockTemplate> block_template;
        try {
            block_template = m_mining.createNewBlock({.coinbase_output_script = coinbase_script});
        } catch (const std::exception& e) {
            LogWarning("CPU miner: failed to create block template: %s", e.what());
        }
        if (!block_template) {
            idle(std::chrono::seconds(1));
            continue;
        }

        CBlock block{block_template->getBlock()};
        block.hashMerkleRoot = BlockMerkleRoot(block);
        const Consensus::Params& consensus{m_chainman.GetConsensus()};
        const int64_t template_start{SteadySeconds()};

        bool found{false};
        uint64_t nonce{slice_begin};
        while (nonce < slice_end && !found && !should_stop()) {
            const uint64_t batch_end{std::min<uint64_t>(nonce + NONCES_PER_CHECK, slice_end)};
            const uint64_t batch_start{nonce};
            for (; nonce < batch_end; ++nonce) {
                block.nNonce = static_cast<uint32_t>(nonce);
                if (CheckProofOfWork(block.GetHash(), block.nBits, consensus)) {
                    found = true;
                    break;
                }
            }
            m_hashes += (nonce - batch_start) + (found ? 1 : 0);

            if (found) break;

            // Abandon stale work: new tip, or template too old.
            const auto current{m_mining.getTip()};
            if (!current || current->hash != tip->hash) break;
            if (SteadySeconds() - template_start >= TEMPLATE_MAX_AGE_SECS) break;
        }

        if (found) {
            const uint256 hash{block.GetHash()};
            LogInfo("CPU miner: found block %s at height %d (thread %d)", hash.GetHex(), tip->height + 1, index);
            if (block_template->submitSolution(block.nVersion, block.nTime, block.nNonce, block.vtx[0])) {
                ++m_blocks_found;
            } else {
                LogWarning("CPU miner: block %s was not accepted", hash.GetHex());
            }
        }
    }
}

} // namespace node
