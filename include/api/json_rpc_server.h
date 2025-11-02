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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "nlohmann/json.hpp"

namespace deo {
namespace node {
    class NodeRuntime;
}

namespace wallet {
    class Wallet;
}

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
// Forward declaration
namespace deo {
namespace node {
    class NodeRuntime;
}
}

class JsonRpcServer {
public:
    /**
     * @brief Constructor
     * @param port Port to listen on
     * @param host Host address to bind to (default: "127.0.0.1")
     * @param node_runtime Optional pointer to NodeRuntime for accessing blockchain state
     * @param username RPC username for authentication (empty = no auth required)
     * @param password RPC password for authentication (empty = no auth required)
     */
    explicit JsonRpcServer(uint16_t port, const std::string& host = "127.0.0.1", 
                          node::NodeRuntime* node_runtime = nullptr,
                          const std::string& username = "",
                          const std::string& password = "");
    
    /**
     * @brief Set NodeRuntime instance
     * @param node_runtime Pointer to NodeRuntime
     */
    void setNodeRuntime(node::NodeRuntime* node_runtime);
    
    /**
     * @brief Set RPC authentication credentials
     * @param username RPC username (empty = disable auth)
     * @param password RPC password (empty = disable auth)
     */
    void setAuthentication(const std::string& username, const std::string& password);
    
    /**
     * @brief Check if authentication is enabled
     * @return True if username and password are set
     */
    bool isAuthenticationEnabled() const;
    
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
    
    /**
     * @brief Get health check response
     * @return Health status as JSON string
     */
    std::string getHealthCheck() const;
    
    /**
     * @brief Get Prometheus metrics format
     * @return Metrics in Prometheus format
     */
    std::string getPrometheusMetrics() const;

private:
    uint16_t port_;                                    ///< Server port
    std::string host_;                                 ///< Server host
    node::NodeRuntime* node_runtime_;                  ///< Pointer to NodeRuntime (for blockchain access)
    std::unique_ptr<wallet::Wallet> wallet_;           ///< Wallet instance for key management
    std::string rpc_username_;                         ///< RPC authentication username
    std::string rpc_password_;                         ///< RPC authentication password
    std::atomic<bool> running_;                        ///< Whether server is running
    std::atomic<bool> stop_requested_;                 ///< Stop request flag
    std::thread server_thread_;                        ///< HTTP server thread
    int server_socket_;                                ///< Server socket file descriptor
    
    // HTTP server methods
    /**
     * @brief HTTP server main loop
     */
    void serverLoop();
    
    /**
     * @brief Handle a single HTTP client connection
     * @param client_socket Client socket file descriptor
     */
    void handleClient(int client_socket);
    
    /**
     * @brief Parse HTTP request headers and extract body
     * @param request HTTP request string
     * @param headers Output map of headers
     * @param body Output request body
     * @param method Output HTTP method (GET, POST, etc.)
     * @param path Output HTTP path
     * @return True if parsing was successful
     */
    bool parseHttpRequest(const std::string& request, std::map<std::string, std::string>& headers, 
                         std::string& body, std::string& method, std::string& path);
    
    /**
     * @brief Create HTTP response
     * @param status_code HTTP status code
     * @param status_message HTTP status message
     * @param content_type Content type
     * @param body Response body
     * @return HTTP response string
     */
    std::string createHttpResponse(int status_code, const std::string& status_message, 
                                   const std::string& content_type, const std::string& body);
    
    /**
     * @brief Parse HTTP Basic Authentication header
     * @param auth_header Authorization header value
     * @param username Output username
     * @param password Output password
     * @return True if valid Basic auth header was parsed
     */
    bool parseBasicAuth(const std::string& auth_header, std::string& username, std::string& password);
    
    /**
     * @brief Check if request is authenticated
     * @param headers HTTP request headers
     * @return True if authenticated (or auth disabled)
     */
    bool checkAuthentication(const std::map<std::string, std::string>& headers);
    
    /**
     * @brief Base64 decode (for Basic auth)
     * @param encoded Base64 encoded string
     * @return Decoded string
     */
    std::string base64Decode(const std::string& encoded);
    
    // Wallet method handlers
    /**
     * @brief Handle wallet_createAccount request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletCreateAccount(const nlohmann::json& params);
    
    /**
     * @brief Handle wallet_importAccount request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletImportAccount(const nlohmann::json& params);
    
    /**
     * @brief Handle wallet_listAccounts request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletListAccounts(const nlohmann::json& params);
    
    /**
     * @brief Handle wallet_exportAccount request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletExportAccount(const nlohmann::json& params);
    
    /**
     * @brief Handle wallet_removeAccount request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletRemoveAccount(const nlohmann::json& params);
    
    /**
     * @brief Handle wallet_setDefaultAccount request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletSetDefaultAccount(const nlohmann::json& params);
    
    /**
     * @brief Handle wallet_getAccount request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletGetAccount(const nlohmann::json& params);
    
    /**
     * @brief Handle wallet_signTransaction request
     * @param params Request parameters
     * @return JSON-RPC response
     */
    JsonRpcResponse handleWalletSignTransaction(const nlohmann::json& params);
    
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
