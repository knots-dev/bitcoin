// Copyright (c) 2026 The Bitcoin Knots developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_CPUMINER_H
#define BITCOIN_NODE_CPUMINER_H

#include <script/script.h>
#include <sync.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

class ChainstateManager;
namespace interfaces {
class Mining;
} // namespace interfaces

namespace node {

/** Default for -gen: do not start the built-in CPU miner at startup. */
static constexpr bool DEFAULT_GENERATE{false};
/** Default for -genproclimit: use a single mining thread. */
static constexpr int DEFAULT_GENPROCLIMIT{1};

/**
 * Built-in CPU miner.
 *
 * Intended for regtest/signet/testnet and for experimentation. On mainnet the
 * expected hashrate of a CPU is negligible; production mining should use
 * getblocktemplate with an external miner, e.g. the DATUM Gateway
 * (see contrib/datum).
 *
 * Each worker thread repeatedly builds a block template paying to the
 * configured coinbase script, scans a private slice of the 32-bit nonce space,
 * and submits the block when a valid proof of work is found. Work is
 * abandoned and rebuilt when the chain tip changes, the nonce slice is
 * exhausted, or the template becomes stale.
 */
class CpuMiner
{
public:
    CpuMiner(ChainstateManager& chainman, interfaces::Mining& mining);
    ~CpuMiner();

    CpuMiner(const CpuMiner&) = delete;
    CpuMiner& operator=(const CpuMiner&) = delete;

    /**
     * (Re)start mining with the given number of threads. A thread count < 0
     * means "one per hardware thread". Any previously running workers are
     * stopped first.
     */
    void Start(int threads, const CScript& coinbase_script);

    /** Stop all worker threads and wait for them to exit. */
    void Stop();

    bool IsRunning() const { return m_running; }
    int Threads() const { return m_running ? m_thread_count.load() : 0; }

    /** Average hashes per second since mining was last started. */
    double HashesPerSec() const;

    /** Number of blocks found by this miner since the node started. */
    uint64_t BlocksFound() const { return m_blocks_found; }

private:
    void Worker(int index, int total, CScript coinbase_script);

    ChainstateManager& m_chainman;
    interfaces::Mining& m_mining;

    Mutex m_mutex;
    std::vector<std::thread> m_workers GUARDED_BY(m_mutex);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_thread_count{0};
    std::atomic<uint64_t> m_hashes{0};
    std::atomic<uint64_t> m_blocks_found{0};
    std::atomic<int64_t> m_start_time{0}; //!< steady clock, seconds
};

/** Resolve the default thread count for a user-supplied -genproclimit value. */
int ResolveCpuMinerThreads(int requested);

} // namespace node

#endif // BITCOIN_NODE_CPUMINER_H
