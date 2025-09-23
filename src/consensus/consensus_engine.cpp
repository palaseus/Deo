/**
 * @file consensus_engine.cpp
 * @brief Consensus mechanism engine implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "consensus/consensus_engine.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include <sstream>

namespace deo {
namespace consensus {

ConsensusEngine::ConsensusEngine(ConsensusType type) 
    : type_(type), is_active_(false) {
    DEO_LOG_DEBUG(CONSENSUS, "Consensus engine created with type: " + std::to_string(static_cast<int>(type)));
}

bool ConsensusEngine::initialize() {
    DEO_LOG_INFO(CONSENSUS, "Initializing consensus engine");
    
    is_active_ = true;
    
    DEO_LOG_INFO(CONSENSUS, "Consensus engine initialized successfully");
    return true;
}

void ConsensusEngine::shutdown() {
    DEO_LOG_INFO(CONSENSUS, "Shutting down consensus engine");
    
    is_active_ = false;
    
    DEO_LOG_INFO(CONSENSUS, "Consensus engine shutdown complete");
}

std::string ConsensusEngine::getStatistics() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"type\": " << static_cast<int>(type_) << ",\n";
    ss << "  \"active\": " << (is_active_ ? "true" : "false") << ",\n";
    ss << "  \"timestamp\": " << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n";
    ss << "}";
    
    return ss.str();
}

} // namespace consensus
} // namespace deo
