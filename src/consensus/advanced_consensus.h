/**
 * @file advanced_consensus.h
 * @brief Advanced consensus mechanisms and optimizations for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_ADVANCED_CONSENSUS_H
#define DEO_ADVANCED_CONSENSUS_H

#include "consensus_engine.h"
#include "proof_of_work.h"
#include "proof_of_stake.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

namespace deo::consensus {

/**
 * @brief Hybrid consensus combining multiple mechanisms
 */
class HybridConsensus : public ConsensusEngine {
public:
    HybridConsensus();
    ~HybridConsensus();
    
    // Consensus engine interface
    bool initialize() override;
    void shutdown() override;
    ConsensusType getType() const override;
    
    // Hybrid consensus operations
    bool addConsensusMechanism(std::shared_ptr<ConsensusEngine> mechanism, double weight);
    bool removeConsensusMechanism(const std::string& mechanism_name);
    void setConsensusWeights(const std::unordered_map<std::string, double>& weights);
    
    // Block validation with hybrid consensus
    bool validateBlock(std::shared_ptr<core::Block> block) override;
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block) override;
    
    // Consensus statistics
    std::unordered_map<std::string, int> getStatistics() const override;
    std::unordered_map<std::string, double> getConsensusWeights() const;
    
private:
    struct ConsensusMechanism {
        std::shared_ptr<ConsensusEngine> engine;
        std::string name;
        double weight;
        std::atomic<uint64_t> votes{0};
        std::atomic<uint64_t> successful_validations{0};
        std::atomic<uint64_t> failed_validations{0};
    };
    
    std::vector<ConsensusMechanism> mechanisms_;
    std::mutex mechanisms_mutex_;
    std::atomic<bool> initialized_;
    
    bool validateWithMechanism(const ConsensusMechanism& mechanism, std::shared_ptr<core::Block> block);
    double calculateConsensusScore(const ConsensusMechanism& mechanism);
};

/**
 * @brief Delegated Proof of Stake (DPoS) consensus
 */
class DelegatedProofOfStake : public ConsensusEngine {
public:
    DelegatedProofOfStake();
    ~DelegatedProofOfStake();
    
    // Consensus engine interface
    bool initialize() override;
    void shutdown() override;
    ConsensusType getType() const override;
    bool validateBlock(std::shared_ptr<core::Block> block) override;
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block) override;
    std::unordered_map<std::string, int> getStatistics() const override;
    
    // DPoS specific operations
    bool addDelegate(const std::string& delegate_address, uint64_t stake_amount);
    bool removeDelegate(const std::string& delegate_address);
    std::vector<std::string> getDelegates() const;
    std::string getCurrentProducer() const;
    uint64_t getDelegateStake(const std::string& delegate_address) const;
    
    // Voting operations
    bool voteForDelegate(const std::string& voter_address, const std::string& delegate_address, uint64_t vote_amount);
    bool unvoteForDelegate(const std::string& voter_address, const std::string& delegate_address);
    std::vector<std::string> getVotedDelegates(const std::string& voter_address) const;
    
    // Producer rotation
    void rotateProducer();
    std::string getNextProducer() const;
    uint64_t getProducerRotationTime() const;
    
private:
    struct Delegate {
        std::string address;
        uint64_t stake_amount;
        uint64_t total_votes;
        std::unordered_map<std::string, uint64_t> voter_votes;
        std::chrono::system_clock::time_point last_production_time;
        std::atomic<uint64_t> blocks_produced{0};
    };
    
    std::unordered_map<std::string, Delegate> delegates_;
    std::vector<std::string> producer_schedule_;
    std::string current_producer_;
    size_t current_producer_index_;
    std::mutex delegates_mutex_;
    std::atomic<bool> initialized_;
    std::chrono::system_clock::time_point last_rotation_time_;
    
    void updateProducerSchedule();
    bool isDelegateEligible(const std::string& delegate_address) const;
    uint64_t calculateDelegateScore(const Delegate& delegate) const;
};

/**
 * @brief Practical Byzantine Fault Tolerance (PBFT) consensus
 */
class PracticalByzantineFaultTolerance : public ConsensusEngine {
public:
    PracticalByzantineFaultTolerance();
    ~PracticalByzantineFaultTolerance();
    
    // Consensus engine interface
    bool initialize() override;
    void shutdown() override;
    ConsensusType getType() const override;
    bool validateBlock(std::shared_ptr<core::Block> block) override;
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block) override;
    std::unordered_map<std::string, int> getStatistics() const override;
    
    // PBFT specific operations
    bool addValidator(const std::string& validator_address, const std::string& public_key);
    bool removeValidator(const std::string& validator_address);
    std::vector<std::string> getValidators() const;
    std::string getPrimaryValidator() const;
    
    // PBFT phases
    bool prePrepare(std::shared_ptr<core::Block> block);
    bool prepare(std::shared_ptr<core::Block> block);
    bool commit(std::shared_ptr<core::Block> block);
    
    // View change
    void initiateViewChange();
    bool isViewChangeInProgress() const;
    uint32_t getCurrentView() const;
    
private:
    struct Validator {
        std::string address;
        std::string public_key;
        std::atomic<bool> is_online{true};
        std::atomic<uint64_t> successful_validations{0};
        std::atomic<uint64_t> failed_validations{0};
        std::chrono::system_clock::time_point last_activity;
    };
    
    enum class PBFTPhase {
        PRE_PREPARE,
        PREPARE,
        COMMIT,
        VIEW_CHANGE
    };
    
    std::unordered_map<std::string, Validator> validators_;
    std::string primary_validator_;
    std::mutex validators_mutex_;
    std::atomic<bool> initialized_;
    std::atomic<PBFTPhase> current_phase_{PBFTPhase::PRE_PREPARE};
    std::atomic<uint32_t> current_view_{0};
    std::atomic<bool> view_change_in_progress_{false};
    
    bool isValidatorOnline(const std::string& validator_address) const;
    bool hasQuorum(const std::unordered_map<std::string, bool>& votes) const;
    void selectNewPrimary();
};

/**
 * @brief Proof of Authority (PoA) consensus
 */
class ProofOfAuthority : public ConsensusEngine {
public:
    ProofOfAuthority();
    ~ProofOfAuthority();
    
    // Consensus engine interface
    bool initialize() override;
    void shutdown() override;
    ConsensusType getType() const override;
    bool validateBlock(std::shared_ptr<core::Block> block) override;
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block) override;
    std::unordered_map<std::string, int> getStatistics() const override;
    
    // PoA specific operations
    bool addAuthority(const std::string& authority_address, const std::string& public_key);
    bool removeAuthority(const std::string& authority_address);
    std::vector<std::string> getAuthorities() const;
    bool isAuthority(const std::string& address) const;
    
    // Authority rotation
    void rotateAuthority();
    std::string getCurrentAuthority() const;
    std::string getNextAuthority() const;
    uint64_t getAuthorityRotationInterval() const;
    
    // Authority validation
    bool validateAuthoritySignature(const std::string& authority_address, 
                                   const std::string& signature, 
                                   const std::string& data) const;
    
private:
    struct Authority {
        std::string address;
        std::string public_key;
        std::chrono::system_clock::time_point last_activity;
        std::atomic<uint64_t> blocks_signed{0};
        std::atomic<bool> is_active{true};
    };
    
    std::unordered_map<std::string, Authority> authorities_;
    std::vector<std::string> authority_schedule_;
    std::string current_authority_;
    size_t current_authority_index_;
    std::mutex authorities_mutex_;
    std::atomic<bool> initialized_;
    std::chrono::system_clock::time_point last_rotation_time_;
    
    void updateAuthoritySchedule();
    bool isAuthorityEligible(const std::string& authority_address) const;
    void rotateToNextAuthority();
};

/**
 * @brief Consensus manager for managing multiple consensus mechanisms
 */
class ConsensusManager {
public:
    ConsensusManager();
    ~ConsensusManager();
    
    // Consensus management
    bool addConsensusEngine(const std::string& name, std::shared_ptr<ConsensusEngine> engine);
    bool removeConsensusEngine(const std::string& name);
    std::shared_ptr<ConsensusEngine> getConsensusEngine(const std::string& name);
    std::vector<std::string> getAvailableConsensusEngines() const;
    
    // Consensus selection
    bool setActiveConsensus(const std::string& name);
    std::string getActiveConsensus() const;
    std::shared_ptr<ConsensusEngine> getActiveConsensusEngine() const;
    
    // Consensus operations
    bool validateBlock(std::shared_ptr<core::Block> block);
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block);
    
    // Consensus statistics
    std::unordered_map<std::string, int> getStatistics() const;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> getAllStatistics() const;
    
private:
    std::unordered_map<std::string, std::shared_ptr<ConsensusEngine>> consensus_engines_;
    std::string active_consensus_;
    std::mutex consensus_mutex_;
    std::atomic<bool> initialized_;
    
    bool isConsensusEngineAvailable(const std::string& name) const;
};

/**
 * @brief Consensus optimizer for performance optimization
 */
class ConsensusOptimizer {
public:
    ConsensusOptimizer();
    ~ConsensusOptimizer();
    
    // Optimization operations
    void optimizeConsensusPerformance();
    void analyzeConsensusMetrics();
    std::vector<std::string> getOptimizationRecommendations() const;
    
    // Consensus tuning
    void tuneConsensusParameters(const std::string& consensus_name, 
                                const std::unordered_map<std::string, double>& parameters);
    std::unordered_map<std::string, double> getOptimalParameters(const std::string& consensus_name) const;
    
    // Performance monitoring
    void startPerformanceMonitoring();
    void stopPerformanceMonitoring();
    std::unordered_map<std::string, double> getPerformanceMetrics() const;
    
private:
    std::unordered_map<std::string, std::unordered_map<std::string, double>> consensus_parameters_;
    std::unordered_map<std::string, double> performance_metrics_;
    std::atomic<bool> monitoring_active_;
    std::vector<std::string> optimization_recommendations_;
    
    void analyzeConsensusPerformance();
    void generateOptimizationRecommendations();
    void updatePerformanceMetrics();
};

} // namespace deo::consensus

#endif // DEO_ADVANCED_CONSENSUS_H
