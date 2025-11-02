/**
 * @file json_rpc_server.cpp
 * @brief JSON-RPC API server implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "api/json_rpc_server.h"
#include "node/node_runtime.h"
#include "wallet/wallet.h"
#include "utils/logger.h"
#include "utils/config.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <algorithm>
#include <regex>
#include <sys/select.h>

namespace deo {
namespace api {

// JsonRpcRequest implementation

bool JsonRpcRequest::fromJson(const nlohmann::json& json) {
    try {
        jsonrpc = json.at("jsonrpc").get<std::string>();
        method = json.at("method").get<std::string>();
        id = json.at("id").get<std::string>();
        
        if (json.contains("params")) {
            params = json.at("params");
        } else {
            params = nlohmann::json::array();
        }
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Failed to parse JSON-RPC request: " + std::string(e.what()));
        return false;
    }
}

nlohmann::json JsonRpcRequest::toJson() const {
    nlohmann::json json;
    json["jsonrpc"] = jsonrpc;
    json["method"] = method;
    json["id"] = id;
    json["params"] = params;
    return json;
}

// JsonRpcResponse implementation

JsonRpcResponse JsonRpcResponse::success(const std::string& request_id, const nlohmann::json& result_data) {
    JsonRpcResponse response;
    response.jsonrpc = "2.0";
    response.result = result_data;
    response.error = nullptr;
    response.id = request_id;
    return response;
}

JsonRpcResponse JsonRpcResponse::createError(const std::string& request_id, int error_code, 
                                            const std::string& error_message, const nlohmann::json& error_data) {
    JsonRpcResponse response;
    response.jsonrpc = "2.0";
    response.result = nullptr;
    
    nlohmann::json error_obj;
    error_obj["code"] = error_code;
    error_obj["message"] = error_message;
    if (!error_data.is_null()) {
        error_obj["data"] = error_data;
    }
    response.error = error_obj;
    response.id = request_id;
    return response;
}

nlohmann::json JsonRpcResponse::toJson() const {
    nlohmann::json json;
    json["jsonrpc"] = jsonrpc;
    json["id"] = id;
    
    if (!result.is_null()) {
        json["result"] = result;
    }
    if (!error.is_null()) {
        json["error"] = error;
    }
    
    return json;
}

// JsonRpcServer implementation

JsonRpcServer::JsonRpcServer(uint16_t port, const std::string& host, node::NodeRuntime* node_runtime,
                              const std::string& username, const std::string& password)
    : port_(port)
    , host_(host)
    , node_runtime_(node_runtime)
    , rpc_username_(username)
    , rpc_password_(password)
    , running_(false)
    , stop_requested_(false)
    , server_socket_(-1)
    , total_requests_(0)
    , total_errors_(0)
    , total_method_calls_(0) {
    
    // Initialize wallet - load config
    wallet::WalletConfig wallet_config;
    wallet_config.data_directory = "./wallet";
    wallet_config.encrypt_wallet = false; // Default
    
    // Try to load wallet encryption setting from config
    std::ifstream config_file("config.json");
    if (config_file.is_open()) {
        try {
            nlohmann::json config_json;
            config_file >> config_json;
            if (config_json.contains("wallet")) {
                const auto& wallet = config_json["wallet"];
                if (wallet.contains("encrypt_wallet")) {
                    wallet_config.encrypt_wallet = wallet["encrypt_wallet"].get<bool>();
                }
                if (wallet.contains("data_directory")) {
                    wallet_config.data_directory = wallet["data_directory"].get<std::string>();
                }
            }
        } catch (const std::exception& e) {
            DEO_LOG_WARNING(CLI, "Failed to read wallet config: " + std::string(e.what()) + ", using defaults");
        }
        config_file.close();
    }
    
    wallet_ = std::make_unique<wallet::Wallet>(wallet_config);
    if (wallet_->initialize()) {
        wallet_->load(""); // Try to load existing wallet
    }
    
    DEO_LOG_DEBUG(CLI, "JsonRpcServer created on " + host + ":" + std::to_string(port));
    if (isAuthenticationEnabled()) {
        DEO_LOG_INFO(CLI, "RPC authentication enabled");
    } else {
        DEO_LOG_WARNING(CLI, "RPC authentication disabled - server is open!");
    }
}

void JsonRpcServer::setNodeRuntime(node::NodeRuntime* node_runtime) {
    node_runtime_ = node_runtime;
    DEO_LOG_DEBUG(CLI, "NodeRuntime set for JsonRpcServer");
}

void JsonRpcServer::setAuthentication(const std::string& username, const std::string& password) {
    rpc_username_ = username;
    rpc_password_ = password;
    if (isAuthenticationEnabled()) {
        DEO_LOG_INFO(CLI, "RPC authentication enabled");
    } else {
        DEO_LOG_WARNING(CLI, "RPC authentication disabled");
    }
}

bool JsonRpcServer::isAuthenticationEnabled() const {
    return !rpc_username_.empty() && !rpc_password_.empty();
}

JsonRpcServer::~JsonRpcServer() {
    // Ensure server is stopped
    if (running_) {
        stop();
    }
    // stop() already joins the thread, but ensure it's joined
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    // stop() already closes socket, but ensure it's closed
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
    DEO_LOG_DEBUG(CLI, "JsonRpcServer destroyed");
}

bool JsonRpcServer::initialize() {
    try {
        registerDefaultMethods();
        DEO_LOG_INFO(CLI, "JsonRpcServer initialized successfully");
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Failed to initialize JsonRpcServer: " + std::string(e.what()));
        return false;
    }
}

bool JsonRpcServer::start() {
    if (running_) {
        DEO_LOG_WARNING(CLI, "JsonRpcServer is already running");
        return true;
    }
    
    DEO_LOG_INFO(CLI, "Starting JsonRpcServer on " + host_ + ":" + std::to_string(port_));
    
    try {
        // Create socket
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0) {
            DEO_LOG_ERROR(CLI, "Failed to create socket: " + std::string(strerror(errno)));
            return false;
        }
        
        // Set socket options: reuse address
        int opt = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            DEO_LOG_ERROR(CLI, "Failed to set socket options: " + std::string(strerror(errno)));
            close(server_socket_);
            server_socket_ = -1;
            return false;
        }
        
        // Set socket to non-blocking mode
        int flags = fcntl(server_socket_, F_GETFL, 0);
        if (flags < 0 || fcntl(server_socket_, F_SETFL, flags | O_NONBLOCK) < 0) {
            DEO_LOG_ERROR(CLI, "Failed to set non-blocking mode: " + std::string(strerror(errno)));
            close(server_socket_);
            server_socket_ = -1;
            return false;
        }
        
        // Bind socket
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        
        if (host_ == "0.0.0.0" || host_.empty()) {
            server_addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
                DEO_LOG_ERROR(CLI, "Invalid host address: " + host_);
                close(server_socket_);
                server_socket_ = -1;
                return false;
            }
        }
        
        if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            DEO_LOG_ERROR(CLI, "Failed to bind socket: " + std::string(strerror(errno)));
            close(server_socket_);
            server_socket_ = -1;
            return false;
        }
        
        // Listen
        if (listen(server_socket_, 10) < 0) {
            DEO_LOG_ERROR(CLI, "Failed to listen on socket: " + std::string(strerror(errno)));
            close(server_socket_);
            server_socket_ = -1;
            return false;
        }
        
        running_ = true;
        stop_requested_ = false;
        
        // Start server thread
        server_thread_ = std::thread(&JsonRpcServer::serverLoop, this);
        
        DEO_LOG_INFO(CLI, "JsonRpcServer started successfully on " + host_ + ":" + std::to_string(port_));
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Failed to start JsonRpcServer: " + std::string(e.what()));
        if (server_socket_ >= 0) {
            close(server_socket_);
            server_socket_ = -1;
        }
        running_ = false;
        return false;
    }
}

void JsonRpcServer::stop() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(CLI, "Stopping JsonRpcServer");
    stop_requested_ = true;
    running_ = false;
    
    // Close socket to wake up accept() call
    if (server_socket_ >= 0) {
        shutdown(server_socket_, SHUT_RDWR);
        close(server_socket_);
        server_socket_ = -1;
    }
    
    // Wait for server thread to finish
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    DEO_LOG_INFO(CLI, "JsonRpcServer stopped");
}

bool JsonRpcServer::isRunning() const {
    return running_;
}

void JsonRpcServer::registerMethod(const std::string& method_name, JsonRpcMethodHandler handler) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    methods_[method_name] = handler;
    DEO_LOG_DEBUG(CLI, "Registered JSON-RPC method: " + method_name);
}

nlohmann::json JsonRpcServer::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    nlohmann::json stats;
    stats["total_requests"] = total_requests_;
    stats["total_errors"] = total_errors_;
    stats["total_method_calls"] = total_method_calls_;
    stats["method_call_counts"] = method_call_counts_;
    stats["registered_methods"] = methods_.size();
    
    return stats;
}

std::string JsonRpcServer::handleRequest(const std::string& request_body) {
    try {
        // Input validation: check request body size (prevent DoS)
        const size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024; // 10MB limit
        if (request_body.size() > MAX_REQUEST_SIZE) {
            DEO_LOG_WARNING(CLI, "JSON-RPC request too large: " + std::to_string(request_body.size()) + " bytes");
            JsonRpcResponse error_response = JsonRpcResponse::createError(
                "null", -32600, "Invalid Request", "Request body too large"
            );
            return error_response.toJson().dump();
        }
        
        // Input validation: check for empty request
        if (request_body.empty()) {
            DEO_LOG_WARNING(CLI, "Empty JSON-RPC request received");
            JsonRpcResponse error_response = JsonRpcResponse::createError(
                "null", -32600, "Invalid Request", "Empty request body"
            );
            return error_response.toJson().dump();
        }
        
        // Parse JSON request with validation
        nlohmann::json request_json;
        try {
            request_json = nlohmann::json::parse(request_body);
        } catch (const nlohmann::json::parse_error& e) {
            DEO_LOG_WARNING(CLI, "JSON parse error: " + std::string(e.what()));
            JsonRpcResponse error_response = JsonRpcResponse::createError(
                "null", -32700, "Parse error", "Invalid JSON: " + std::string(e.what())
            );
            return error_response.toJson().dump();
        }
        
        JsonRpcRequest request;
        if (!request.fromJson(request_json)) {
        JsonRpcResponse error_response = JsonRpcResponse::createError(
            "null", -32700, "Parse error", "Invalid JSON-RPC request format"
        );
            return error_response.toJson().dump();
        }
        
        // Process request
        JsonRpcResponse response = processRequest(request);
        return response.toJson().dump();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error handling JSON-RPC request: " + std::string(e.what()));
        JsonRpcResponse error_response = JsonRpcResponse::createError(
            "null", -32603, "Internal error", e.what()
        );
        return error_response.toJson().dump();
    }
}

JsonRpcResponse JsonRpcServer::processRequest(const JsonRpcRequest& request) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    
    auto it = methods_.find(request.method);
    if (it == methods_.end()) {
        updateStatistics(request.method, false);
        return JsonRpcResponse::createError(
            request.id, -32601, "Method not found", "Method '" + request.method + "' not found"
        );
    }
    
    try {
        JsonRpcResponse response = it->second(request.params);
        updateStatistics(request.method, response.error.is_null());
        return response;
    } catch (const std::exception& e) {
        updateStatistics(request.method, false);
        return JsonRpcResponse::createError(
            request.id, -32603, "Internal error", e.what()
        );
    }
}

void JsonRpcServer::updateStatistics(const std::string& method_name, bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    total_requests_++;
    total_method_calls_++;
    
    if (!success) {
        total_errors_++;
    }
    
    method_call_counts_[method_name]++;
}

void JsonRpcServer::registerDefaultMethods() {
    // Register blockchain-related methods
    registerMethod("getNodeInfo", [this](const nlohmann::json& params) {
        return handleGetNodeInfo(params);
    });
    
    registerMethod("getBlockchainInfo", [this](const nlohmann::json& params) {
        return handleGetBlockchainInfo(params);
    });
    
    registerMethod("getContractInfo", [this](const nlohmann::json& params) {
        return handleGetContractInfo(params);
    });
    
    registerMethod("deployContract", [this](const nlohmann::json& params) {
        return handleDeployContract(params);
    });
    
    registerMethod("callContract", [this](const nlohmann::json& params) {
        return handleCallContract(params);
    });
    
    registerMethod("getBalance", [this](const nlohmann::json& params) {
        return handleGetBalance(params);
    });
    
    registerMethod("getTransaction", [this](const nlohmann::json& params) {
        return handleGetTransaction(params);
    });
    
    registerMethod("getBlock", [this](const nlohmann::json& params) {
        return handleGetBlock(params);
    });
    
    registerMethod("getMempoolInfo", [this](const nlohmann::json& params) {
        return handleGetMempoolInfo(params);
    });
    
    registerMethod("replayBlock", [this](const nlohmann::json& params) {
        return handleReplayBlock(params);
    });
    
    // Register networking and peer management methods
    registerMethod("getNetworkInfo", [this](const nlohmann::json& params) {
        return handleGetNetworkInfo(params);
    });
    
    registerMethod("getPeers", [this](const nlohmann::json& params) {
        return handleGetPeers(params);
    });
    
    registerMethod("connectPeer", [this](const nlohmann::json& params) {
        return handleConnectPeer(params);
    });
    
    registerMethod("disconnectPeer", [this](const nlohmann::json& params) {
        return handleDisconnectPeer(params);
    });
    
    registerMethod("broadcastTransaction", [this](const nlohmann::json& params) {
        return handleBroadcastTransaction(params);
    });
    
    registerMethod("broadcastBlock", [this](const nlohmann::json& params) {
        return handleBroadcastBlock(params);
    });
    
    registerMethod("syncChain", [this](const nlohmann::json& params) {
        return handleSyncChain(params);
    });
    
    registerMethod("getNetworkStats", [this](const nlohmann::json& params) {
        return handleGetNetworkStats(params);
    });
    
    // Wallet methods
    registerMethod("wallet_createAccount", [this](const nlohmann::json& params) {
        return handleWalletCreateAccount(params);
    });
    
    registerMethod("wallet_importAccount", [this](const nlohmann::json& params) {
        return handleWalletImportAccount(params);
    });
    
    registerMethod("wallet_listAccounts", [this](const nlohmann::json& params) {
        return handleWalletListAccounts(params);
    });
    
    registerMethod("wallet_exportAccount", [this](const nlohmann::json& params) {
        return handleWalletExportAccount(params);
    });
    
    registerMethod("wallet_removeAccount", [this](const nlohmann::json& params) {
        return handleWalletRemoveAccount(params);
    });
    
    registerMethod("wallet_setDefaultAccount", [this](const nlohmann::json& params) {
        return handleWalletSetDefaultAccount(params);
    });
    
    registerMethod("wallet_getAccount", [this](const nlohmann::json& params) {
        return handleWalletGetAccount(params);
    });
    
    registerMethod("wallet_signTransaction", [this](const nlohmann::json& params) {
        return handleWalletSignTransaction(params);
    });
    
    // Web3-compatible methods
    registerMethod("eth_blockNumber", [this](const nlohmann::json& params) {
        return handleEthBlockNumber(params);
    });
    
    registerMethod("eth_getBalance", [this](const nlohmann::json& params) {
        return handleEthGetBalance(params);
    });
    
    registerMethod("eth_getBlockByNumber", [this](const nlohmann::json& params) {
        return handleEthGetBlockByNumber(params);
    });
    
    registerMethod("eth_getBlockByHash", [this](const nlohmann::json& params) {
        return handleEthGetBlockByHash(params);
    });
    
    registerMethod("eth_getTransactionByHash", [this](const nlohmann::json& params) {
        return handleEthGetTransactionByHash(params);
    });
    
    registerMethod("eth_getTransactionReceipt", [this](const nlohmann::json& params) {
        return handleEthGetTransactionReceipt(params);
    });
    
    registerMethod("eth_call", [this](const nlohmann::json& params) {
        return handleEthCall(params);
    });
    
    registerMethod("eth_sendRawTransaction", [this](const nlohmann::json& params) {
        return handleEthSendRawTransaction(params);
    });
    
    registerMethod("eth_estimateGas", [this](const nlohmann::json& params) {
        return handleEthEstimateGas(params);
    });
    
    registerMethod("eth_gasPrice", [this](const nlohmann::json& params) {
        return handleEthGasPrice(params);
    });
    
    registerMethod("eth_getCode", [this](const nlohmann::json& params) {
        return handleEthGetCode(params);
    });
    
    registerMethod("eth_getStorageAt", [this](const nlohmann::json& params) {
        return handleEthGetStorageAt(params);
    });
    
    registerMethod("net_version", [this](const nlohmann::json& params) {
        return handleNetVersion(params);
    });
    
    registerMethod("net_listening", [this](const nlohmann::json& params) {
        return handleNetListening(params);
    });
    
    registerMethod("net_peerCount", [this](const nlohmann::json& params) {
        return handleNetPeerCount(params);
    });
    
    registerMethod("web3_clientVersion", [this](const nlohmann::json& params) {
        return handleWeb3ClientVersion(params);
    });
}

// Default method handlers

JsonRpcResponse JsonRpcServer::handleGetNodeInfo(const nlohmann::json& /* params */) {
    nlohmann::json result;
    result["node_id"] = "deo_node_001";
    result["version"] = "1.0.0";
    result["network"] = "deo_mainnet";
    result["status"] = (running_ ? "running" : "stopped");
    
    if (node_runtime_) {
        auto stats = node_runtime_->getStatistics();
        result["is_mining"] = stats.is_mining;
        result["is_syncing"] = stats.is_syncing;
        result["blockchain_height"] = stats.blockchain_height;
        result["mempool_size"] = stats.mempool_size;
        result["transactions_per_second"] = stats.transactions_per_second;
        result["avg_block_time_seconds"] = stats.avg_block_time_seconds;
        
        // Calculate uptime based on when server started (simplified - uses current time)
        // In a full implementation, would track actual server start time
        auto now = std::chrono::system_clock::now();
        auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        result["uptime_seconds"] = uptime_seconds;
    } else {
        result["uptime_seconds"] = 0;
        result["is_mining"] = false;
        result["is_syncing"] = false;
        result["blockchain_height"] = 0;
        result["mempool_size"] = 0;
        result["transactions_per_second"] = 0.0;
        result["avg_block_time_seconds"] = 0.0;
    }
    
    result["api_version"] = "2.0";
    
    return JsonRpcResponse::success("1", result);
}

JsonRpcResponse JsonRpcServer::handleGetNetworkStats(const nlohmann::json& /* params */) {
    try {
        nlohmann::json result;
        
        if (node_runtime_) {
            auto stats = node_runtime_->getStatistics();
            
            // Network stats
            if (node_runtime_->getP2PNetworkManager()) {
                auto p2p_network = node_runtime_->getP2PNetworkManager();
                if (p2p_network && p2p_network->isRunning()) {
                    auto network_stats = p2p_network->getNetworkStats();
                    result["network"] = nlohmann::json::object();
                    result["network"]["connected_peers"] = p2p_network->getPeerCount();
                    result["network"]["messages_sent"] = network_stats.tcp_stats.messages_sent;
                    result["network"]["messages_received"] = network_stats.tcp_stats.messages_received;
                    result["network"]["bytes_sent"] = network_stats.tcp_stats.bytes_sent;
                    result["network"]["bytes_received"] = network_stats.tcp_stats.bytes_received;
                }
            }
            
            // Performance metrics
            result["performance"] = nlohmann::json::object();
            result["performance"]["transactions_per_second"] = stats.transactions_per_second;
            result["performance"]["avg_block_time_seconds"] = stats.avg_block_time_seconds;
            result["performance"]["sync_speed_blocks_per_sec"] = stats.sync_speed_blocks_per_sec;
            result["performance"]["total_storage_operations"] = stats.total_storage_operations;
            result["performance"]["total_network_messages"] = stats.total_network_messages;
            
            // Blockchain stats
            result["blockchain"] = nlohmann::json::object();
            result["blockchain"]["height"] = stats.blockchain_height;
            result["blockchain"]["total_transactions"] = stats.transactions_processed;
            result["blockchain"]["total_gas_used"] = stats.total_gas_used;
            result["blockchain"]["mempool_size"] = stats.mempool_size;
            
            return JsonRpcResponse::success("1", result);
        }
        
        // Default response
        result["performance"] = nlohmann::json::object();
        result["blockchain"] = nlohmann::json::object();
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get network stats: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleGetBlockchainInfo(const nlohmann::json& /* params */) {
    if (!node_runtime_) {
        // Return default values if NodeRuntime not available
        nlohmann::json result;
        result["chain_id"] = "deo_mainnet";
        result["block_height"] = 0;
        result["best_block_hash"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
        result["total_transactions"] = 0;
        result["total_contracts"] = 0;
        result["gas_price"] = 20;
        result["block_gas_limit"] = 10000000;
        return JsonRpcResponse::success("1", result);
    }
    
    try {
        auto blockchain_state = node_runtime_->getBlockchainState();
        auto node_stats = node_runtime_->getStatistics();
        
        nlohmann::json result;
        result["chain_id"] = "deo_mainnet";
        result["block_height"] = blockchain_state.height;
        result["best_block_hash"] = "0x" + blockchain_state.best_block_hash;
        result["total_transactions"] = blockchain_state.total_transactions;
        result["total_contracts"] = node_stats.contracts_deployed;
        
        // Load gas price and block gas limit from config
        uint64_t gas_price = 20; // Default
        uint64_t block_gas_limit = 10000000; // Default
        std::ifstream config_file("config.json");
        if (config_file.is_open()) {
            try {
                nlohmann::json config_json;
                config_file >> config_json;
                if (config_json.contains("blockchain")) {
                    const auto& blockchain = config_json["blockchain"];
                    if (blockchain.contains("gas_price")) {
                        gas_price = blockchain["gas_price"].get<uint64_t>();
                    }
                    if (blockchain.contains("block_gas_limit")) {
                        block_gas_limit = blockchain["block_gas_limit"].get<uint64_t>();
                    }
                }
            } catch (const std::exception&) {
                // Use defaults on error
            }
            config_file.close();
        }
        result["gas_price"] = gas_price;
        result["block_gas_limit"] = block_gas_limit;
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error getting blockchain info: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleGetContractInfo(const nlohmann::json& params) {
    if (!params.contains("address") || !params["address"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "address parameter is required");
    }
    
    std::string address = params["address"].get<std::string>();
    
    if (!node_runtime_) {
        // Return placeholder data if NodeRuntime not available
        nlohmann::json result;
        result["address"] = address;
        result["bytecode_size"] = 0;
        result["balance"] = "0x0";
        result["nonce"] = 0;
        result["is_deployed"] = false;
        result["storage_entries"] = 0;
        return JsonRpcResponse::success("1", result);
    }
    
    try {
        // Use NodeRuntime's getContractInfo method
        std::string contract_info_json = node_runtime_->getContractInfo(address);
        nlohmann::json contract_info = nlohmann::json::parse(contract_info_json);
        
        if (contract_info.contains("error")) {
            return JsonRpcResponse::createError("1", -32603, "Contract not found", contract_info["error"]);
        }
        
        return JsonRpcResponse::success("1", contract_info);
    } catch (const nlohmann::json::parse_error& e) {
        DEO_LOG_ERROR(CLI, "Error parsing contract info: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to parse contract info");
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error getting contract info: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleDeployContract(const nlohmann::json& params) {
    if (!params.contains("bytecode") || !params["bytecode"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "bytecode parameter is required");
    }
    
    std::string bytecode = params["bytecode"].get<std::string>();
    
    // For now, return placeholder data
    nlohmann::json result;
    result["contract_address"] = "0x" + std::string(40, '0');
    result["transaction_hash"] = "0x" + std::string(64, '0');
    result["gas_used"] = 100000;
    result["status"] = "success";
    
    return JsonRpcResponse::success("1", result);
}

JsonRpcResponse JsonRpcServer::handleCallContract(const nlohmann::json& params) {
    if (!params.contains("address") || !params["address"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "address parameter is required");
    }
    
    if (!params.contains("data") || !params["data"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "data parameter is required");
    }
    
    std::string address = params["address"].get<std::string>();
    std::string data = params["data"].get<std::string>();
    
    // For now, return placeholder data
    nlohmann::json result;
    result["return_data"] = "0x";
    result["gas_used"] = 21000;
    result["status"] = "success";
    
    return JsonRpcResponse::success("1", result);
}

JsonRpcResponse JsonRpcServer::handleGetBalance(const nlohmann::json& params) {
    if (!params.contains("address") || !params["address"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "address parameter is required");
    }
    
    std::string address = params["address"].get<std::string>();
    
    if (!node_runtime_) {
        // Return placeholder if NodeRuntime not available
        nlohmann::json result;
        result["address"] = address;
        result["balance"] = "0x0";
        result["balance_wei"] = "0";
        return JsonRpcResponse::success("1", result);
    }
    
    try {
        uint64_t balance = node_runtime_->getBalance(address);
        
        // Convert to hex string for Web3 compatibility
        std::stringstream hex_ss;
        hex_ss << "0x" << std::hex << balance;
        std::string balance_hex = hex_ss.str();
        
        nlohmann::json result;
        result["address"] = address;
        result["balance"] = balance_hex;
        result["balance_wei"] = std::to_string(balance);
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error getting balance: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleGetTransaction(const nlohmann::json& params) {
    if (!params.contains("hash") || !params["hash"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "hash parameter is required");
    }
    
    std::string hash = params["hash"].get<std::string>();
    
    if (!node_runtime_) {
        nlohmann::json result;
        result["hash"] = hash;
        result["status"] = "not_found";
        return JsonRpcResponse::success("1", result);
    }
    
    try {
        auto transaction = node_runtime_->getTransaction(hash);
        
        if (!transaction) {
            nlohmann::json result;
            result["hash"] = hash;
            result["status"] = "not_found";
            return JsonRpcResponse::success("1", result);
        }
        
        // Convert transaction to JSON
        nlohmann::json result = transaction->toJson();
        result["status"] = "found";
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error getting transaction: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleGetBlock(const nlohmann::json& params) {
    if (!params.contains("hash") || !params["hash"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "hash parameter is required");
    }
    
    std::string hash = params["hash"].get<std::string>();
    
    if (!node_runtime_) {
        nlohmann::json result;
        result["hash"] = hash;
        result["status"] = "not_found";
        return JsonRpcResponse::success("1", result);
    }
    
    try {
        auto block = node_runtime_->getBlock(hash);
        
        if (!block) {
            nlohmann::json result;
            result["hash"] = hash;
            result["status"] = "not_found";
            return JsonRpcResponse::success("1", result);
        }
        
        // Convert block to JSON
        nlohmann::json result = block->toJson();
        result["status"] = "found";
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error getting block: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleGetMempoolInfo(const nlohmann::json& /* params */) {
    if (!node_runtime_) {
        nlohmann::json result;
        result["pending_transactions"] = 0;
        result["mempool_size"] = 0;
        result["gas_price"] = 20;
        return JsonRpcResponse::success("1", result);
    }
    
    try {
        size_t mempool_size = node_runtime_->getMempoolSize();
        auto mempool_txs = node_runtime_->getMempoolTransactions(100); // Get up to 100 transactions
        
        nlohmann::json result;
        result["pending_transactions"] = mempool_txs.size();
        result["mempool_size"] = mempool_size;
        // Load gas price from config
        uint64_t gas_price = 20; // Default
        std::ifstream config_file("config.json");
        if (config_file.is_open()) {
            try {
                nlohmann::json config_json;
                config_file >> config_json;
                if (config_json.contains("blockchain") && config_json["blockchain"].contains("gas_price")) {
                    gas_price = config_json["blockchain"]["gas_price"].get<uint64_t>();
                }
            } catch (const std::exception&) {
                // Use default on error
            }
            config_file.close();
        }
        result["gas_price"] = gas_price;
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error getting mempool info: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleReplayBlock(const nlohmann::json& params) {
    if (!params.contains("hash") || !params["hash"].is_string()) {
        return JsonRpcResponse::createError("1", -32602, "Invalid params", "hash parameter is required");
    }
    
    std::string hash = params["hash"].get<std::string>();
    
    if (!node_runtime_) {
        nlohmann::json result;
        result["block_hash"] = hash;
        result["validation_success"] = false;
        result["error"] = "NodeRuntime not available";
        return JsonRpcResponse::success("1", result);
    }
    
    try {
        std::string replay_result_json = node_runtime_->replayBlock(hash);
        nlohmann::json replay_result = nlohmann::json::parse(replay_result_json);
        
        if (replay_result.contains("error")) {
            return JsonRpcResponse::createError("1", -32603, replay_result["error"], replay_result);
        }
        
        return JsonRpcResponse::success("1", replay_result);
    } catch (const nlohmann::json::parse_error& e) {
        DEO_LOG_ERROR(CLI, "Error parsing replay result: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to parse replay result");
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error replaying block: " + std::string(e.what()));
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

// Networking and peer management methods

JsonRpcResponse JsonRpcServer::handleGetNetworkInfo(const nlohmann::json& /* params */) {
    try {
        nlohmann::json result;
        result["network_id"] = "deo_mainnet";
        result["protocol_version"] = "1.0.0";
        result["node_id"] = "node_" + std::to_string(std::hash<std::string>{}(std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count())));
        result["listening"] = running_.load();
        result["local_address"] = host_;
        result["local_port"] = port_;
        result["connected_peers"] = 0;
        result["network_enabled"] = false;
        
        if (node_runtime_) {
            auto p2p_network = node_runtime_->getP2PNetworkManager();
            if (p2p_network && p2p_network->isRunning()) {
                auto network_stats = p2p_network->getNetworkStats();
                result["connected_peers"] = p2p_network->getPeerCount();
                result["network_enabled"] = true;
                result["p2p_port"] = p2p_network->getConnectedPeers().size(); // Placeholder, actual port would come from config
                result["messages_sent"] = network_stats.tcp_stats.messages_sent;
                result["messages_received"] = network_stats.tcp_stats.messages_received;
                result["bytes_sent"] = network_stats.tcp_stats.bytes_sent;
                result["bytes_received"] = network_stats.tcp_stats.bytes_received;
            }
        }
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get network info: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleGetPeers(const nlohmann::json& /* params */) {
    try {
        nlohmann::json result;
        result["peers"] = nlohmann::json::array();
        result["total"] = 0;
        result["connected"] = 0;
        result["max_peers"] = 50;
        result["network_enabled"] = false;
        
        if (node_runtime_) {
            auto p2p_network = node_runtime_->getP2PNetworkManager();
            if (p2p_network && p2p_network->isRunning()) {
                auto peer_addresses = p2p_network->getConnectedPeers();
                result["total"] = peer_addresses.size();
                result["connected"] = peer_addresses.size();
                result["network_enabled"] = true;
                
                for (const auto& peer_addr : peer_addresses) {
                    nlohmann::json peer_info;
                    // peer_addr is in format "address:port" or just "address"
                    size_t colon_pos = peer_addr.find(':');
                    if (colon_pos != std::string::npos) {
                        peer_info["address"] = peer_addr.substr(0, colon_pos);
                        peer_info["port"] = std::stoul(peer_addr.substr(colon_pos + 1));
                    } else {
                        peer_info["address"] = peer_addr;
                    }
                    result["peers"].push_back(peer_info);
                }
            }
        }
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get peers: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleConnectPeer(const nlohmann::json& params) {
    try {
        if (!params.contains("address") || !params.contains("port")) {
            return JsonRpcResponse::createError("connect_peer", -32602, "Missing required parameters: address, port");
        }
        
        std::string address = params["address"].get<std::string>();
        uint16_t port = params["port"].get<uint16_t>();
        
        bool connected = false;
        if (node_runtime_) {
            auto p2p_network = node_runtime_->getP2PNetworkManager();
            if (p2p_network && p2p_network->isRunning()) {
                connected = p2p_network->connectToPeer(address, port);
            }
        }
        
        nlohmann::json result;
        result["address"] = address;
        result["port"] = port;
        result["connected"] = connected;
        result["message"] = connected ? "Peer connection successful" : "Peer connection failed or network not enabled";
        
        return JsonRpcResponse::success("connect_peer", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("connect_peer", -32603, "Failed to connect to peer: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleDisconnectPeer(const nlohmann::json& params) {
    try {
        if (!params.contains("address") || !params.contains("port")) {
            return JsonRpcResponse::createError("disconnect_peer", -32602, "Missing required parameters: address, port");
        }
        
        std::string address = params["address"].get<std::string>();
        uint16_t port = params["port"].get<uint16_t>();
        
        nlohmann::json result;
        result["address"] = address;
        result["port"] = port;
        result["disconnected"] = false;
        
        if (node_runtime_) {
            auto p2p_network = node_runtime_->getP2PNetworkManager();
            if (p2p_network && p2p_network->isRunning()) {
                p2p_network->disconnectFromPeer(address, port);
                result["disconnected"] = true;
                result["message"] = "Peer disconnection requested";
            } else {
                result["message"] = "P2P network not enabled or not running";
            }
        } else {
            result["message"] = "Node runtime not available";
        }
        
        return JsonRpcResponse::success("disconnect_peer", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("disconnect_peer", -32603, "Failed to disconnect peer: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleBroadcastTransaction(const nlohmann::json& params) {
    try {
        if (!params.contains("transaction_data")) {
            return JsonRpcResponse::createError("broadcast_transaction", -32602, "Missing required parameter: transaction_data");
        }
        
        std::string transaction_data = params["transaction_data"].get<std::string>();
        
        // If NodeRuntime is available, try to broadcast via network
        // For now, return success with transaction data
        // In full implementation, this would:
        // 1. Parse transaction from JSON
        // 2. Validate transaction
        // 3. Add to local mempool
        // 4. Broadcast via GossipProtocol if network is enabled
        
        nlohmann::json result;
        result["transaction_id"] = ""; // Would be transaction hash
        result["broadcasted"] = true;
        result["peers_reached"] = 0; // Would be actual peer count
        
        // Try to parse and validate transaction
        try {
            nlohmann::json tx_json = nlohmann::json::parse(transaction_data);
            auto tx = std::make_shared<core::Transaction>();
            if (tx->fromJson(tx_json)) {
                result["transaction_id"] = tx->getId();
                result["valid"] = tx->validate();
                
                // If node_runtime_ is available and has mempool, add transaction
                if (node_runtime_) {
                    if (node_runtime_->addTransaction(tx)) {
                        result["added_to_mempool"] = true;
                    }
                }
            }
        } catch (const std::exception& e) {
            DEO_LOG_WARNING(CLI, "Failed to parse transaction for broadcast: " + std::string(e.what()));
        }
        
        return JsonRpcResponse::success("broadcast_transaction", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("broadcast_transaction", -32603, "Failed to broadcast transaction: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleBroadcastBlock(const nlohmann::json& params) {
    try {
        if (!params.contains("block_data")) {
            return JsonRpcResponse::createError("broadcast_block", -32602, "Missing required parameter: block_data");
        }
        
        std::string block_data = params["block_data"].get<std::string>();
        
        nlohmann::json result;
        result["block_hash"] = "";
        result["broadcasted"] = true;
        result["peers_reached"] = 0;
        
        // Try to parse and validate block
        try {
            nlohmann::json block_json = nlohmann::json::parse(block_data);
            auto block = std::make_shared<core::Block>();
            if (block->fromJson(block_json)) {
                result["block_hash"] = block->calculateHash();
                result["block_height"] = block->getHeight();
                result["valid"] = block->validate();
                
                // Note: In full implementation, this would broadcast via GossipProtocol
                // For now, we just validate the block structure
            }
        } catch (const std::exception& e) {
            DEO_LOG_WARNING(CLI, "Failed to parse block for broadcast: " + std::string(e.what()));
        }
        
        return JsonRpcResponse::success("broadcast_block", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("broadcast_block", -32603, "Failed to broadcast block: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleSyncChain(const nlohmann::json& /* params */) {
    try {
        nlohmann::json result;
        result["sync_started"] = false;
        result["current_height"] = 0;
        result["target_height"] = 0;
        result["progress"] = "0%";
        
        if (node_runtime_) {
            auto blockchain_state = node_runtime_->getBlockchainState();
            result["current_height"] = blockchain_state.height;
            result["is_syncing"] = blockchain_state.is_syncing;
            result["sync_started"] = blockchain_state.is_syncing;
            
            // Get sync status from NodeRuntime
            auto sync_status = node_runtime_->getSyncStatus();
            result["sync_status"] = static_cast<int>(sync_status);
            result["sync_progress"] = node_runtime_->getSyncProgress();
            
            auto stats = node_runtime_->getStatistics();
            result["target_height"] = stats.target_sync_height;
            result["current_sync_height"] = stats.current_sync_height;
            result["sync_speed_blocks_per_sec"] = stats.sync_speed_blocks_per_sec;
            
            if (sync_status != sync::SyncStatus::IDLE) {
                result["sync_started"] = true;
            }
        }
        
        result["message"] = "Chain synchronization status";
        
        return JsonRpcResponse::success("sync_chain", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("sync_chain", -32603, "Failed to sync chain: " + std::string(e.what()));
    }
}

// Duplicate removed - handleGetNetworkStats is defined above with enhanced performance metrics

// Web3-compatible method implementations

JsonRpcResponse JsonRpcServer::handleEthBlockNumber(const nlohmann::json& /* params */) {
    try {
        if (!node_runtime_) {
            // Return default if NodeRuntime not available
            return JsonRpcResponse::success("1", "0x0");
        }
        
        // Get current block height from blockchain state
        auto blockchain_state = node_runtime_->getBlockchainState();
        std::string block_number = "0x" + 
            [](uint64_t n) {
                std::stringstream ss;
                ss << std::hex << n;
                return ss.str();
            }(blockchain_state.height);
        
        return JsonRpcResponse::success("1", block_number);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get block number: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGetBalance(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 2) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string address = params[0].get<std::string>();
        std::string block_tag = params[1].get<std::string>(); // "latest", "earliest", "pending", or block number
        
        if (!node_runtime_) {
            return JsonRpcResponse::success("1", "0x0");
        }
        
        // Handle block tag
        uint64_t balance = 0;
        if (block_tag == "latest" || block_tag == "pending") {
            // Get balance from current state
            balance = node_runtime_->getBalance(address);
        } else if (block_tag == "earliest") {
            // For earliest, return 0 (genesis block balance)
            balance = 0;
        } else {
            // Try to parse as block number
            try {
                if (block_tag.substr(0, 2) == "0x") {
                    std::stoull(block_tag.substr(2), nullptr, 16); // Parse but don't use yet
                } else {
                    std::stoull(block_tag); // Parse but don't use yet
                }
                // For now, just return current balance (full block state access would require blockchain state at specific height)
                // TODO: Implement full historical state access for specific block heights
                balance = node_runtime_->getBalance(address);
            } catch (const std::exception&) {
                // Invalid block tag, default to latest
                balance = node_runtime_->getBalance(address);
            }
        }
        std::stringstream hex_ss;
        hex_ss << "0x" << std::hex << balance;
        std::string balance_hex = hex_ss.str();
        
        return JsonRpcResponse::success("1", balance_hex);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get balance: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGetBlockByNumber(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 2) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string block_number_str = params[0].get<std::string>();
        bool include_transactions = params[1].get<bool>();
        
        if (!node_runtime_) {
            nlohmann::json result;
            result["number"] = block_number_str;
            result["hash"] = "0x" + std::string(64, '0');
            result["parentHash"] = "0x" + std::string(64, '0');
            result["timestamp"] = "0x0";
            result["transactions"] = nlohmann::json::array();
            return JsonRpcResponse::success("1", result);
        }
        
        // Parse block number (hex string like "0x1" or "latest", "earliest", "pending")
        uint64_t block_number = 0;
        if (block_number_str == "latest" || block_number_str == "pending") {
            auto blockchain_state = node_runtime_->getBlockchainState();
            block_number = blockchain_state.height;
        } else if (block_number_str == "earliest") {
            block_number = 0;
        } else {
            // Remove "0x" prefix if present
            if (block_number_str.substr(0, 2) == "0x") {
                block_number_str = block_number_str.substr(2);
            }
            block_number = std::stoull(block_number_str, nullptr, 16);
        }
        
        auto block = node_runtime_->getBlock(block_number);
        if (!block) {
            return JsonRpcResponse::success("1", nullptr); // null for not found
        }
        
        // Convert to Web3-compatible format
        nlohmann::json result;
        result["number"] = "0x" + [](uint64_t n) {
            std::stringstream ss;
            ss << std::hex << n;
            return ss.str();
        }(block->getHeader().height);
        result["hash"] = "0x" + block->getHash();
        result["parentHash"] = "0x" + block->getHeader().previous_hash;
        
        std::stringstream ts_ss;
        ts_ss << "0x" << std::hex << block->getHeader().timestamp;
        result["timestamp"] = ts_ss.str();
        result["transactions"] = nlohmann::json::array();
        
        if (include_transactions) {
            for (const auto& tx : block->getTransactions()) {
                result["transactions"].push_back(tx->toJson());
            }
        } else {
            for (const auto& tx : block->getTransactions()) {
                result["transactions"].push_back("0x" + tx->getId());
            }
        }
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get block: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGetBlockByHash(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 2) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string block_hash = params[0].get<std::string>();
        bool include_transactions = params[1].get<bool>();
        
        // Remove "0x" prefix if present
        if (block_hash.substr(0, 2) == "0x") {
            block_hash = block_hash.substr(2);
        }
        
        if (!node_runtime_) {
            return JsonRpcResponse::success("1", nullptr); // null for not found
        }
        
        auto block = node_runtime_->getBlock(block_hash);
        if (!block) {
            return JsonRpcResponse::success("1", nullptr); // null for not found
        }
        
        // Convert to Web3-compatible format
        nlohmann::json result;
        result["number"] = "0x" + [](uint64_t n) {
            std::stringstream ss;
            ss << std::hex << n;
            return ss.str();
        }(block->getHeader().height);
        result["hash"] = "0x" + block->getHash();
        result["parentHash"] = "0x" + block->getHeader().previous_hash;
        
        std::stringstream ts_ss;
        ts_ss << "0x" << std::hex << block->getHeader().timestamp;
        result["timestamp"] = ts_ss.str();
        result["transactions"] = nlohmann::json::array();
        
        if (include_transactions) {
            for (const auto& tx : block->getTransactions()) {
                result["transactions"].push_back(tx->toJson());
            }
        } else {
            for (const auto& tx : block->getTransactions()) {
                result["transactions"].push_back("0x" + tx->getId());
            }
        }
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get block: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGetTransactionByHash(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 1) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string tx_hash = params[0].get<std::string>();
        
        // Remove "0x" prefix if present
        if (tx_hash.substr(0, 2) == "0x") {
            tx_hash = tx_hash.substr(2);
        }
        
        if (!node_runtime_) {
            return JsonRpcResponse::success("1", nullptr); // null for not found
        }
        
        auto transaction = node_runtime_->getTransaction(tx_hash);
        if (!transaction) {
            return JsonRpcResponse::success("1", nullptr); // null for not found
        }
        
        // Convert transaction to Web3-compatible format
        nlohmann::json tx_json = transaction->toJson();
        
        // Web3 format expects specific fields
        nlohmann::json result;
        result["hash"] = "0x" + transaction->getId();
        result["from"] = tx_json.value("from_address", "0x" + std::string(40, '0'));
        result["to"] = tx_json.value("to_address", "0x" + std::string(40, '0'));
        
        // Convert value to hex
        uint64_t value = 0;
        if (tx_json.contains("value")) {
            if (tx_json["value"].is_string()) {
                value = std::stoull(tx_json["value"].get<std::string>(), nullptr, 16);
            } else {
                value = tx_json["value"].get<uint64_t>();
            }
        }
        std::stringstream value_ss;
        value_ss << "0x" << std::hex << value;
        result["value"] = value_ss.str();
        
        // Calculate gas limit from transaction (estimate based on transaction size)
        // Base gas cost: 21000 for simple transfer
        uint64_t gas_limit = 21000;
        gas_limit += transaction->getInputs().size() * 100;
        gas_limit += transaction->getOutputs().size() * 50;
        gas_limit += transaction->getSize() * 2; // 2 gas per byte
        
        // Get gas price from config
        uint64_t gas_price = 20; // Default
        uint64_t config_gas_price = 20;
        std::ifstream config_file("config.json");
        if (config_file.is_open()) {
            try {
                nlohmann::json config_json;
                config_file >> config_json;
                if (config_json.contains("blockchain")) {
                    const auto& blockchain = config_json["blockchain"];
                    if (blockchain.contains("gas_price")) {
                        config_gas_price = blockchain["gas_price"].get<uint64_t>();
                    }
                }
            } catch (const std::exception&) {
                // Use default on error
            }
            config_file.close();
        }
        gas_price = config_gas_price;
        
        std::stringstream gas_ss;
        gas_ss << "0x" << std::hex << gas_limit;
        result["gas"] = gas_ss.str();
        
        std::stringstream gas_price_ss;
        gas_price_ss << "0x" << std::hex << gas_price;
        result["gasPrice"] = gas_price_ss.str();
        
        result["nonce"] = tx_json.value("nonce", 0);
        result["blockNumber"] = tx_json.value("block_number", "0x0");
        result["blockHash"] = tx_json.value("block_hash", "0x" + std::string(64, '0'));
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get transaction: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGetTransactionReceipt(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 1) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string tx_hash = params[0].get<std::string>();
        
        nlohmann::json result;
        result["transactionHash"] = tx_hash;
        result["blockNumber"] = "0x1";
        result["blockHash"] = "0x" + std::string(64, '0');
        result["gasUsed"] = "0x5208";
        result["status"] = "0x1";
        
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get transaction receipt: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthCall(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 2) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        nlohmann::json call_object = params[0];
        std::string block_tag = params[1].get<std::string>();
        
        // Return call result in hex format
        std::string result = "0x"; // Placeholder - should execute contract call
        return JsonRpcResponse::success("1", result);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to call contract: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthSendRawTransaction(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 1) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string raw_tx = params[0].get<std::string>();
        
        // Return transaction hash
        std::string tx_hash = "0x" + std::string(64, '0');
        return JsonRpcResponse::success("1", tx_hash);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to send transaction: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthEstimateGas(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 1) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        nlohmann::json call_object = params[0];
        
        // Return gas estimate in hex format
        std::string gas_estimate = "0x5208"; // 21000 gas
        return JsonRpcResponse::success("1", gas_estimate);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to estimate gas: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGasPrice(const nlohmann::json& /* params */) {
    try {
        // Return gas price in hex format (wei)
        std::string gas_price = "0x4a817c800"; // 20 gwei
        return JsonRpcResponse::success("1", gas_price);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get gas price: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGetCode(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 2) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string address = params[0].get<std::string>();
        std::string block_tag = params[1].get<std::string>();
        
        // Return contract code in hex format
        std::string code = "0x"; // Placeholder - should get from blockchain
        return JsonRpcResponse::success("1", code);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get code: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleEthGetStorageAt(const nlohmann::json& params) {
    try {
        if (!params.is_array() || params.size() < 3) {
            return JsonRpcResponse::createError("1", -32602, "Invalid parameters");
        }
        
        std::string address = params[0].get<std::string>();
        std::string position = params[1].get<std::string>();
        std::string block_tag = params[2].get<std::string>();
        
        // Return storage value in hex format
        std::string value = "0x0";
        return JsonRpcResponse::success("1", value);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get storage: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleNetVersion(const nlohmann::json& /* params */) {
    try {
        // Return network version
        std::string version = "1"; // Deo mainnet
        return JsonRpcResponse::success("1", version);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get network version: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleNetListening(const nlohmann::json& /* params */) {
    try {
        // Return listening status
        bool listening = true;
        return JsonRpcResponse::success("1", listening);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get listening status: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleNetPeerCount(const nlohmann::json& /* params */) {
    try {
        // Return peer count in hex format
        std::string peer_count = "0x0";
        return JsonRpcResponse::success("1", peer_count);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get peer count: " + std::string(e.what()));
    }
}

JsonRpcResponse JsonRpcServer::handleWeb3ClientVersion(const nlohmann::json& /* params */) {
    try {
        // Return client version
        std::string version = "Deo/1.0.0";
        return JsonRpcResponse::success("1", version);
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Failed to get client version: " + std::string(e.what()));
    }
}

void JsonRpcServer::serverLoop() {
    while (running_ && !stop_requested_) {
        // Check stop flag first before doing any socket operations
        if (stop_requested_ || !running_) {
            break;
        }
        
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        // Accept connections with timeout (check stop_requested_ periodically)
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket_, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout for faster stop response
        
        int select_result = select(server_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (select_result < 0) {
            if (errno == EINTR || stop_requested_) {
                continue;
            }
            DEO_LOG_ERROR(CLI, "Select error in server loop: " + std::string(strerror(errno)));
            break;
        }
        
        if (select_result > 0 && FD_ISSET(server_socket_, &read_fds)) {
            int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_addr_len);
            
            if (client_socket < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                if (stop_requested_) {
                    break;
                }
                DEO_LOG_WARNING(CLI, "Accept error: " + std::string(strerror(errno)));
                continue;
            }
            
            // Handle client in same thread (simple implementation)
            // In production, should use thread pool or spawn threads
            handleClient(client_socket);
            close(client_socket);
        }
    }
    
    DEO_LOG_DEBUG(CLI, "HTTP server loop exited");
}

void JsonRpcServer::handleClient(int client_socket) {
    try {
        // Read HTTP request (simple implementation - assumes request fits in buffer)
        char buffer[8192];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            return;
        }
        
        buffer[bytes_received] = '\0';
        std::string request(buffer);
        
        // Parse HTTP request
        std::map<std::string, std::string> headers;
        std::string body;
        std::string http_path;
        std::string http_method;
        
        if (!parseHttpRequest(request, headers, body, http_method, http_path)) {
            std::string error_response = createHttpResponse(400, "Bad Request", 
                "text/plain", "Invalid HTTP request");
            send(client_socket, error_response.c_str(), error_response.size(), 0);
            return;
        }
        
        // Handle special endpoints (no auth required for health/metrics)
        if (http_path == "/health" || http_path == "/health/") {
            std::string health_response = getHealthCheck();
            std::string http_response = createHttpResponse(200, "OK", 
                "application/json", health_response);
            send(client_socket, http_response.c_str(), http_response.size(), 0);
            return;
        }
        
        if (http_path == "/metrics" || http_path == "/metrics/") {
            std::string metrics_response = getPrometheusMetrics();
            std::string http_response = createHttpResponse(200, "OK", 
                "text/plain; version=0.0.4", metrics_response);
            send(client_socket, http_response.c_str(), http_response.size(), 0);
            return;
        }
        
        // Check authentication for JSON-RPC endpoints
        if (!checkAuthentication(headers)) {
            std::string error_response = createHttpResponse(401, "Unauthorized", 
                "text/plain", "Authentication required");
            send(client_socket, error_response.c_str(), error_response.size(), 0);
            return;
        }
        
        // Check for Content-Length header to read full body
        if (headers.find("content-length") != headers.end()) {
            size_t content_length = std::stoul(headers["content-length"]);
            if (content_length > body.size()) {
                // Read remaining body
                size_t remaining = content_length - body.size();
                std::vector<char> remaining_buffer(remaining);
                ssize_t additional = recv(client_socket, remaining_buffer.data(), remaining, 0);
                if (additional > 0) {
                    body += std::string(remaining_buffer.data(), additional);
                }
            }
        }
        
        // Process JSON-RPC request
        std::string json_response = handleRequest(body);
        
        // Create HTTP response
        std::string http_response = createHttpResponse(200, "OK", 
            "application/json", json_response);
        
        // Send response
        send(client_socket, http_response.c_str(), http_response.size(), 0);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CLI, "Error handling client: " + std::string(e.what()));
        std::string error_response = createHttpResponse(500, "Internal Server Error",
            "text/plain", "Internal server error");
        send(client_socket, error_response.c_str(), error_response.size(), 0);
    }
}

bool JsonRpcServer::parseHttpRequest(const std::string& request, 
                                    std::map<std::string, std::string>& headers, 
                                    std::string& body, std::string& method, std::string& path) {
    // Simple HTTP parser - handles basic GET/POST requests
    std::istringstream request_stream(request);
    std::string line;
    
    // Parse request line
    if (!std::getline(request_stream, line)) {
        return false;
    }
    
    // Remove \r if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    // Parse request line: "METHOD PATH HTTP/VERSION"
    std::istringstream request_line_stream(line);
    std::string http_version;
    if (!(request_line_stream >> method >> path >> http_version)) {
        return false;
    }
    
    // Parse headers
    bool found_empty_line = false;
    while (std::getline(request_stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            found_empty_line = true;
            break;
        }
        
        // Parse header: "Name: Value"
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string header_name = line.substr(0, colon_pos);
            std::string header_value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            header_name.erase(0, header_name.find_first_not_of(" \t"));
            header_name.erase(header_name.find_last_not_of(" \t") + 1);
            header_value.erase(0, header_value.find_first_not_of(" \t"));
            header_value.erase(header_value.find_last_not_of(" \t") + 1);
            
            // Convert to lowercase for case-insensitive comparison
            std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
            
            headers[header_name] = header_value;
        }
    }
    
    // Read body (what's left in the stream)
    if (found_empty_line) {
        std::ostringstream body_stream;
        body_stream << request_stream.rdbuf();
        body = body_stream.str();
    }
    
    return true;
}

std::string JsonRpcServer::createHttpResponse(int status_code, 
                                              const std::string& status_message,
                                              const std::string& content_type,
                                              const std::string& body) {
    std::ostringstream response;
    
    response << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";  // CORS support
    
    // Add WWW-Authenticate header for 401 responses
    if (status_code == 401) {
        response << "WWW-Authenticate: Basic realm=\"Deo Blockchain RPC\"\r\n";
    }
    
    response << "\r\n";
    response << body;
    
    return response.str();
}

bool JsonRpcServer::parseBasicAuth(const std::string& auth_header, std::string& username, std::string& password) {
    // Basic auth format: "Basic base64(username:password)"
    if (auth_header.substr(0, 6) != "Basic ") {
        return false;
    }
    
    std::string encoded = auth_header.substr(6);
    std::string decoded = base64Decode(encoded);
    
    // Find the colon separator
    size_t colon_pos = decoded.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    username = decoded.substr(0, colon_pos);
    password = decoded.substr(colon_pos + 1);
    
    return true;
}

bool JsonRpcServer::checkAuthentication(const std::map<std::string, std::string>& headers) {
    // If authentication is disabled, allow all requests
    if (!isAuthenticationEnabled()) {
        return true;
    }
    
    // Look for Authorization header
    auto auth_it = headers.find("authorization");
    if (auth_it == headers.end()) {
        DEO_LOG_WARNING(CLI, "RPC request without Authorization header");
        return false;
    }
    
    std::string username, password;
    if (!parseBasicAuth(auth_it->second, username, password)) {
        DEO_LOG_WARNING(CLI, "Invalid Basic auth header format");
        return false;
    }
    
    // Verify credentials
    if (username != rpc_username_ || password != rpc_password_) {
        DEO_LOG_WARNING(CLI, "Invalid RPC credentials from " + username);
        return false;
    }
    
    return true;
}

std::string JsonRpcServer::base64Decode(const std::string& encoded) {
    // Simple Base64 decoder - maps characters to values
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    int val = 0, valb = -8;
    
    for (unsigned char c : encoded) {
        if (c == '=') break;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        
        size_t pos = chars.find(c);
        if (pos == std::string::npos) {
            return ""; // Invalid character
        }
        
        val = (val << 6) + static_cast<int>(pos);
        valb += 6;
        
        if (valb >= 0) {
            decoded.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

// Wallet method implementations

JsonRpcResponse JsonRpcServer::handleWalletCreateAccount(const nlohmann::json& params) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        std::string label = "";
        if (params.is_object() && params.contains("label")) {
            label = params["label"].get<std::string>();
        }
        
        std::string address = wallet_->createAccount(label);
        
        if (address.empty()) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to create account");
        }
        
        // Save wallet
        wallet_->save("");
        
        nlohmann::json result;
        result["address"] = address;
        result["label"] = label.empty() ? "" : label;
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleWalletImportAccount(const nlohmann::json& params) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        if (!params.is_object() || !params.contains("private_key")) {
            return JsonRpcResponse::createError("1", -32602, "Invalid params", "private_key parameter required");
        }
        
        std::string private_key = params["private_key"].get<std::string>();
        std::string label = "";
        std::string password = "";
        
        if (params.contains("label")) {
            label = params["label"].get<std::string>();
        }
        if (params.contains("password")) {
            password = params["password"].get<std::string>();
        }
        
        std::string address = wallet_->importAccount(private_key, label, password);
        
        if (address.empty()) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to import account");
        }
        
        // Save wallet
        wallet_->save("");
        
        nlohmann::json result;
        result["address"] = address;
        result["label"] = label.empty() ? "" : label;
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleWalletListAccounts(const nlohmann::json& /* params */) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        auto accounts = wallet_->listAccounts();
        std::string default_account = wallet_->getDefaultAccount();
        
        nlohmann::json result;
        result["accounts"] = nlohmann::json::array();
        result["default_account"] = default_account;
        result["count"] = accounts.size();
        
        for (const auto& address : accounts) {
            auto account_info = wallet_->getAccount(address);
            nlohmann::json account_json;
            account_json["address"] = address;
            
            if (account_info) {
                account_json["label"] = account_info->label;
                account_json["is_encrypted"] = account_info->is_encrypted;
            }
            
            account_json["is_default"] = (address == default_account);
            result["accounts"].push_back(account_json);
        }
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleWalletExportAccount(const nlohmann::json& params) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        if (!params.is_object() || !params.contains("address")) {
            return JsonRpcResponse::createError("1", -32602, "Invalid params", "address parameter required");
        }
        
        std::string address = params["address"].get<std::string>();
        std::string password = "";
        if (params.contains("password")) {
            password = params["password"].get<std::string>();
        }
        
        auto exported = wallet_->exportAccount(address, password);
        
        if (exported.empty()) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to export account");
        }
        
        return JsonRpcResponse::success("1", exported);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleWalletRemoveAccount(const nlohmann::json& params) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        if (!params.is_object() || !params.contains("address")) {
            return JsonRpcResponse::createError("1", -32602, "Invalid params", "address parameter required");
        }
        
        std::string address = params["address"].get<std::string>();
        std::string password = "";
        if (params.contains("password")) {
            password = params["password"].get<std::string>();
        }
        
        if (!wallet_->removeAccount(address, password)) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to remove account");
        }
        
        // Save wallet
        wallet_->save("");
        
        nlohmann::json result;
        result["address"] = address;
        result["removed"] = true;
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleWalletSetDefaultAccount(const nlohmann::json& params) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        // If no address provided, return current default
        if (!params.is_object() || !params.contains("address")) {
            std::string default_addr = wallet_->getDefaultAccount();
            nlohmann::json result;
            result["default_account"] = default_addr.empty() ? nullptr : default_addr;
            return JsonRpcResponse::success("1", result);
        }
        
        std::string address = params["address"].get<std::string>();
        
        if (!wallet_->setDefaultAccount(address)) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to set default account");
        }
        
        nlohmann::json result;
        result["default_account"] = address;
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleWalletGetAccount(const nlohmann::json& params) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        if (!params.is_object() || !params.contains("address")) {
            return JsonRpcResponse::createError("1", -32602, "Invalid params", "address parameter required");
        }
        
        std::string address = params["address"].get<std::string>();
        auto account_info = wallet_->getAccount(address);
        
        if (!account_info) {
            return JsonRpcResponse::createError("1", -32603, "Account not found", "Account not found: " + address);
        }
        
        nlohmann::json result;
        result["address"] = account_info->address;
        result["label"] = account_info->label;
        result["public_key"] = account_info->public_key;
        result["is_encrypted"] = account_info->is_encrypted;
        
        std::string default_account = wallet_->getDefaultAccount();
        result["is_default"] = (address == default_account);
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

JsonRpcResponse JsonRpcServer::handleWalletSignTransaction(const nlohmann::json& params) {
    try {
        if (!wallet_) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Wallet not initialized");
        }
        
        if (!params.is_object() || !params.contains("transaction") || !params.contains("address")) {
            return JsonRpcResponse::createError("1", -32602, "Invalid params", "transaction and address parameters required");
        }
        
        std::string address = params["address"].get<std::string>();
        std::string password = "";
        if (params.contains("password")) {
            password = params["password"].get<std::string>();
        }
        
        // Parse transaction from JSON
        nlohmann::json tx_json = params["transaction"];
        auto transaction = std::make_shared<core::Transaction>();
        if (!transaction->fromJson(tx_json)) {
            return JsonRpcResponse::createError("1", -32602, "Invalid params", "Invalid transaction format");
        }
        
        // Sign transaction
        if (!wallet_->signTransaction(*transaction, address, password)) {
            return JsonRpcResponse::createError("1", -32603, "Internal error", "Failed to sign transaction");
        }
        
        // Return signed transaction
        nlohmann::json result = transaction->toJson();
        result["signed"] = true;
        
        return JsonRpcResponse::success("1", result);
        
    } catch (const std::exception& e) {
        return JsonRpcResponse::createError("1", -32603, "Internal error", e.what());
    }
}

std::string JsonRpcServer::getHealthCheck() const {
    nlohmann::json health;
    health["status"] = "healthy";
    health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    bool all_healthy = true;
    
    // Check RPC server status
    health["rpc_server"] = nlohmann::json::object();
    health["rpc_server"]["running"] = running_.load();
    health["rpc_server"]["port"] = port_;
    
    if (!running_) {
        all_healthy = false;
    }
    
    // Check NodeRuntime status
    if (node_runtime_) {
        health["node_runtime"] = nlohmann::json::object();
        health["node_runtime"]["available"] = true;
        health["node_runtime"]["running"] = node_runtime_->isRunning();
        
        if (node_runtime_->isRunning()) {
            auto stats = node_runtime_->getStatistics();
            auto blockchain_state = node_runtime_->getBlockchainState();
            
            health["node_runtime"]["blockchain_height"] = blockchain_state.height;
            health["node_runtime"]["is_syncing"] = stats.is_syncing;
            health["node_runtime"]["is_mining"] = stats.is_mining;
            health["node_runtime"]["mempool_size"] = stats.mempool_size;
            
            // Check if syncing indicates potential issues
            if (stats.is_syncing && stats.sync_progress < 50 && stats.sync_speed_blocks_per_sec < 0.1) {
                all_healthy = false;
                health["warnings"] = nlohmann::json::array();
                health["warnings"].push_back("Sync progress is slow");
            }
        } else {
            all_healthy = false;
        }
    } else {
        health["node_runtime"] = nlohmann::json::object();
        health["node_runtime"]["available"] = false;
        // NodeRuntime not required for basic health
    }
    
    // Check wallet status
    health["wallet"] = nlohmann::json::object();
    if (wallet_) {
        health["wallet"]["initialized"] = true;
    } else {
        health["wallet"]["initialized"] = false;
        // Wallet not critical for basic health
    }
    
    // Overall health status
    if (all_healthy) {
        health["status"] = "healthy";
    } else {
        health["status"] = "degraded";
    }
    
    return health.dump(2);
}

std::string JsonRpcServer::getPrometheusMetrics() const {
    std::stringstream metrics;
    
    // Add metric type comments (Prometheus format)
    metrics << "# HELP deo_rpc_requests_total Total number of RPC requests\n";
    metrics << "# TYPE deo_rpc_requests_total counter\n";
    metrics << "deo_rpc_requests_total " << total_requests_ << "\n";
    
    metrics << "# HELP deo_rpc_errors_total Total number of RPC errors\n";
    metrics << "# TYPE deo_rpc_errors_total counter\n";
    metrics << "deo_rpc_errors_total " << total_errors_ << "\n";
    
    metrics << "# HELP deo_rpc_method_calls_total Total number of method calls\n";
    metrics << "# TYPE deo_rpc_method_calls_total counter\n";
    metrics << "deo_rpc_method_calls_total " << total_method_calls_ << "\n";
    
    // Per-method call counts
    metrics << "# HELP deo_rpc_method_calls Method call count by method name\n";
    metrics << "# TYPE deo_rpc_method_calls counter\n";
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        for (const auto& [method_name, count] : method_call_counts_) {
            metrics << "deo_rpc_method_calls{method=\"" << method_name << "\"} " << count << "\n";
        }
    }
    
    // NodeRuntime metrics if available
    if (node_runtime_ && node_runtime_->isRunning()) {
        auto stats = node_runtime_->getStatistics();
        
        metrics << "# HELP deo_blockchain_height Current blockchain height\n";
        metrics << "# TYPE deo_blockchain_height gauge\n";
        metrics << "deo_blockchain_height " << stats.blockchain_height << "\n";
        
        metrics << "# HELP deo_mempool_size Current mempool size\n";
        metrics << "# TYPE deo_mempool_size gauge\n";
        metrics << "deo_mempool_size " << stats.mempool_size << "\n";
        
        metrics << "# HELP deo_transactions_per_second Current transactions per second\n";
        metrics << "# TYPE deo_transactions_per_second gauge\n";
        metrics << "deo_transactions_per_second " << stats.transactions_per_second << "\n";
        
        metrics << "# HELP deo_avg_block_time_seconds Average block time in seconds\n";
        metrics << "# TYPE deo_avg_block_time_seconds gauge\n";
        metrics << "deo_avg_block_time_seconds " << stats.avg_block_time_seconds << "\n";
        
        metrics << "# HELP deo_blocks_mined_total Total blocks mined\n";
        metrics << "# TYPE deo_blocks_mined_total counter\n";
        metrics << "deo_blocks_mined_total " << stats.blocks_mined << "\n";
        
        metrics << "# HELP deo_transactions_processed_total Total transactions processed\n";
        metrics << "# TYPE deo_transactions_processed_total counter\n";
        metrics << "deo_transactions_processed_total " << stats.transactions_processed << "\n";
        
        metrics << "# HELP deo_contracts_deployed_total Total contracts deployed\n";
        metrics << "# TYPE deo_contracts_deployed_total counter\n";
        metrics << "deo_contracts_deployed_total " << stats.contracts_deployed << "\n";
        
        metrics << "# HELP deo_total_gas_used_total Total gas used\n";
        metrics << "# TYPE deo_total_gas_used_total counter\n";
        metrics << "deo_total_gas_used_total " << stats.total_gas_used << "\n";
        
        metrics << "# HELP deo_is_mining Whether node is currently mining\n";
        metrics << "# TYPE deo_is_mining gauge\n";
        metrics << "deo_is_mining " << (stats.is_mining ? 1 : 0) << "\n";
        
        metrics << "# HELP deo_is_syncing Whether node is currently syncing\n";
        metrics << "# TYPE deo_is_syncing gauge\n";
        metrics << "deo_is_syncing " << (stats.is_syncing ? 1 : 0) << "\n";
        
        metrics << "# HELP deo_sync_progress_percent Sync progress percentage\n";
        metrics << "# TYPE deo_sync_progress_percent gauge\n";
        metrics << "deo_sync_progress_percent " << stats.sync_progress << "\n";
        
        metrics << "# HELP deo_sync_speed_blocks_per_sec Sync speed in blocks per second\n";
        metrics << "# TYPE deo_sync_speed_blocks_per_sec gauge\n";
        metrics << "deo_sync_speed_blocks_per_sec " << stats.sync_speed_blocks_per_sec << "\n";
        
        metrics << "# HELP deo_network_messages_total Total network messages\n";
        metrics << "# TYPE deo_network_messages_total counter\n";
        metrics << "deo_network_messages_total " << stats.total_network_messages << "\n";
        
        metrics << "# HELP deo_storage_operations_total Total storage operations\n";
        metrics << "# TYPE deo_storage_operations_total counter\n";
        metrics << "deo_storage_operations_total " << stats.total_storage_operations << "\n";
        
        // Network peer count if P2P available
        if (node_runtime_->getP2PNetworkManager()) {
            auto p2p_network = node_runtime_->getP2PNetworkManager();
            if (p2p_network && p2p_network->isRunning()) {
                auto network_stats = p2p_network->getNetworkStats();
                
                metrics << "# HELP deo_peer_count Current number of connected peers\n";
                metrics << "# TYPE deo_peer_count gauge\n";
                metrics << "deo_peer_count " << p2p_network->getPeerCount() << "\n";
                
                metrics << "# HELP deo_network_messages_sent_total Total network messages sent\n";
                metrics << "# TYPE deo_network_messages_sent_total counter\n";
                metrics << "deo_network_messages_sent_total " << network_stats.tcp_stats.messages_sent << "\n";
                
                metrics << "# HELP deo_network_messages_received_total Total network messages received\n";
                metrics << "# TYPE deo_network_messages_received_total counter\n";
                metrics << "deo_network_messages_received_total " << network_stats.tcp_stats.messages_received << "\n";
                
                metrics << "# HELP deo_network_bytes_sent_total Total bytes sent\n";
                metrics << "# TYPE deo_network_bytes_sent_total counter\n";
                metrics << "deo_network_bytes_sent_total " << network_stats.tcp_stats.bytes_sent << "\n";
                
                metrics << "# HELP deo_network_bytes_received_total Total bytes received\n";
                metrics << "# TYPE deo_network_bytes_received_total counter\n";
                metrics << "deo_network_bytes_received_total " << network_stats.tcp_stats.bytes_received << "\n";
            }
        }
    }
    
    return metrics.str();
}

} // namespace api
} // namespace deo
