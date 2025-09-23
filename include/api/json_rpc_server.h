/**
 * @file json_rpc_server.h
 * @brief JSON-RPC API server for programmatic blockchain access
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <functional>
#include <vector>

#include "nlohmann/json.hpp"

namespace deo {
namespace api {

/**
 * @brief JSON-RPC request structure
 */
struct JsonRpcRequest {
    std::string jsonrpc;           ///< JSON-RPC version (should be "2.0")
    std::string method;            ///< Method name
    nlohmann::json params;         ///< Method parameters
    std::string id;                ///< Request ID
    
    /**
     * @brief Parse from JSON
     * @param json JSON object
     * @return True if parsing was successful
     */
    bool fromJson(const nlohmann::json& json);
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json toJson() const;
};

/**
 * @brief JSON-RPC response structure
 */
struct JsonRpcResponse {
    std::string jsonrpc;           ///< JSON-RPC version (should be "2.0")
    nlohmann::json result;         ///< Result data (null if error)
    nlohmann::json error;          ///< Error data (null if success)
    std::string id;                ///< Request ID
    
    /**
     * @brief Create success response
     * @param request_id Request ID
     * @param result_data Result data
     * @return Success response
     */
    static JsonRpcResponse success(const std::string& request_id, const nlohmann::json& result_data);
    
    /**
     * @brief Create error response
     * @param request_id Request ID
     * @param error_code Error code
     * @param error_message Error message
     * @param error_data Additional error data (optional)
     * @return Error response
     */
    static JsonRpcResponse createError(const std::string& request_id, int error_code, 
                                     const std::string& error_message, const nlohmann::json& error_data = nullptr);
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json toJson() const;
};

/**
 * @brief JSON-RPC method handler function type
 */
using JsonRpcMethodHandler = std::function<JsonRpcResponse(const nlohmann::json& params)>;

/**
 * @brief JSON-RPC server for blockchain API access
 * 
 * This server provides a JSON-RPC 2.0 compatible API for programmatic
 * access to blockchain functionality including contract deployment,
 * contract calls, state queries, and node management.
 */
class JsonRpcServer {
public:
    /**
     * @brief Constructor
     * @param port Port to listen on
     * @param host Host address to bind to (default: "127.0.0.1")
     */
    explicit JsonRpcServer(uint16_t port, const std::string& host = "127.0.0.1");
    
    /**
     * @brief Destructor
     */
    ~JsonRpcServer();
    
    /**
     * @brief Initialize the server
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Start the server
     * @return True if started successfully
     */
    bool start();
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Check if server is running
     * @return True if running
     */
    bool isRunning() const;
    
    /**
     * @brief Register a JSON-RPC method handler
     * @param method_name Method name
     * @param handler Handler function
     */
    void registerMethod(const std::string& method_name, JsonRpcMethodHandler handler);
    
    /**
     * @brief Get server statistics
     * @return Statistics as JSON
     */
    nlohmann::json getStatistics() const;

private:
    uint16_t port_;                                    ///< Server port
    std::string host_;                                 ///< Server host
    std::atomic<bool> running_;                        ///< Whether server is running
    std::atomic<bool> stop_requested_;                 ///< Stop request flag
    
    // Method handlers
    std::map<std::string, JsonRpcMethodHandler> methods_; ///< Registered method handlers
    mutable std::mutex methods_mutex_;                 ///< Methods mutex
    
    // Statistics
    mutable std::mutex stats_mutex_;                   ///< Statistics mutex
    uint64_t total_requests_;                          ///< Total requests processed
    uint64_t total_errors_;                            ///< Total errors
    uint64_t total_method_calls_;                      ///< Total method calls
    std::map<std::string, uint64_t> method_call_counts_; ///< Method call counts
    
    /**
     * @brief Handle incoming HTTP request
     * @param request_body Request body
     * @return Response body
     */
    std::string handleRequest(const std::string& request_body);
    
    /**
     * @brief Process JSON-RPC request
     * @param request JSON-RPC request
     * @return JSON-RPC response
     */
    JsonRpcResponse processRequest(const JsonRpcRequest& request);
    
    /**
     * @brief Update statistics
     * @param method_name Method name that was called
     * @param success Whether the call was successful
     */
    void updateStatistics(const std::string& method_name, bool success);
    
    /**
     * @brief Register default blockchain methods
     */
    void registerDefaultMethods();
    
    // Default method handlers
    JsonRpcResponse handleGetNodeInfo(const nlohmann::json& params);
    JsonRpcResponse handleGetBlockchainInfo(const nlohmann::json& params);
    JsonRpcResponse handleGetContractInfo(const nlohmann::json& params);
    JsonRpcResponse handleDeployContract(const nlohmann::json& params);
    JsonRpcResponse handleCallContract(const nlohmann::json& params);
    JsonRpcResponse handleGetBalance(const nlohmann::json& params);
    JsonRpcResponse handleGetTransaction(const nlohmann::json& params);
    JsonRpcResponse handleGetBlock(const nlohmann::json& params);
    JsonRpcResponse handleGetMempoolInfo(const nlohmann::json& params);
    JsonRpcResponse handleReplayBlock(const nlohmann::json& params);
    
    // Networking and peer management methods
    JsonRpcResponse handleGetNetworkInfo(const nlohmann::json& params);
    JsonRpcResponse handleGetPeers(const nlohmann::json& params);
    JsonRpcResponse handleConnectPeer(const nlohmann::json& params);
    JsonRpcResponse handleDisconnectPeer(const nlohmann::json& params);
    JsonRpcResponse handleBroadcastTransaction(const nlohmann::json& params);
    JsonRpcResponse handleBroadcastBlock(const nlohmann::json& params);
    JsonRpcResponse handleSyncChain(const nlohmann::json& params);
    JsonRpcResponse handleGetNetworkStats(const nlohmann::json& params);
    
    // Web3-compatible method handlers
    JsonRpcResponse handleEthBlockNumber(const nlohmann::json& params);
    JsonRpcResponse handleEthGetBalance(const nlohmann::json& params);
    JsonRpcResponse handleEthGetBlockByNumber(const nlohmann::json& params);
    JsonRpcResponse handleEthGetBlockByHash(const nlohmann::json& params);
    JsonRpcResponse handleEthGetTransactionByHash(const nlohmann::json& params);
    JsonRpcResponse handleEthGetTransactionReceipt(const nlohmann::json& params);
    JsonRpcResponse handleEthCall(const nlohmann::json& params);
    JsonRpcResponse handleEthSendRawTransaction(const nlohmann::json& params);
    JsonRpcResponse handleEthEstimateGas(const nlohmann::json& params);
    JsonRpcResponse handleEthGasPrice(const nlohmann::json& params);
    JsonRpcResponse handleEthGetCode(const nlohmann::json& params);
    JsonRpcResponse handleEthGetStorageAt(const nlohmann::json& params);
    JsonRpcResponse handleNetVersion(const nlohmann::json& params);
    JsonRpcResponse handleNetListening(const nlohmann::json& params);
    JsonRpcResponse handleNetPeerCount(const nlohmann::json& params);
    JsonRpcResponse handleWeb3ClientVersion(const nlohmann::json& params);
};

} // namespace api
} // namespace deo
