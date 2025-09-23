#!/bin/bash

# Deo Blockchain Web3 API Examples using cURL
# This script demonstrates how to interact with the Deo blockchain
# using the Web3-compatible JSON-RPC API via command line.

# Configuration
RPC_URL="http://localhost:8545"
CONTENT_TYPE="Content-Type: application/json"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to make JSON-RPC requests
make_request() {
    local method="$1"
    local params="$2"
    local id="${3:-1}"
    
    local payload="{\"jsonrpc\":\"2.0\",\"method\":\"$method\",\"params\":$params,\"id\":$id}"
    
    curl -s -X POST \
        -H "$CONTENT_TYPE" \
        -d "$payload" \
        "$RPC_URL"
}

# Function to format JSON output
format_json() {
    echo "$1" | python3 -m json.tool 2>/dev/null || echo "$1"
}

echo -e "${BLUE}=== Deo Blockchain Web3 API Examples ===${NC}\n"

# Test 1: Get client version
echo -e "${YELLOW}1. Getting client version...${NC}"
response=$(make_request "web3_clientVersion" "[]")
echo "Response:"
format_json "$response"
echo

# Test 2: Get network version
echo -e "${YELLOW}2. Getting network version...${NC}"
response=$(make_request "net_version" "[]")
echo "Response:"
format_json "$response"
echo

# Test 3: Check if node is listening
echo -e "${YELLOW}3. Checking if node is listening...${NC}"
response=$(make_request "net_listening" "[]")
echo "Response:"
format_json "$response"
echo

# Test 4: Get peer count
echo -e "${YELLOW}4. Getting peer count...${NC}"
response=$(make_request "net_peerCount" "[]")
echo "Response:"
format_json "$response"
echo

# Test 5: Get current block number
echo -e "${YELLOW}5. Getting current block number...${NC}"
response=$(make_request "eth_blockNumber" "[]")
echo "Response:"
format_json "$response"
echo

# Test 6: Get gas price
echo -e "${YELLOW}6. Getting current gas price...${NC}"
response=$(make_request "eth_gasPrice" "[]")
echo "Response:"
format_json "$response"
echo

# Test 7: Get block by number
echo -e "${YELLOW}7. Getting block by number (latest)...${NC}"
response=$(make_request "eth_getBlockByNumber" "[\"latest\", false]")
echo "Response:"
format_json "$response"
echo

# Test 8: Get account balance
echo -e "${YELLOW}8. Getting account balance...${NC}"
test_address="0x1234567890123456789012345678901234567890"
response=$(make_request "eth_getBalance" "[\"$test_address\", \"latest\"]")
echo "Response:"
format_json "$response"
echo

# Test 9: Get contract code
echo -e "${YELLOW}9. Getting contract code...${NC}"
contract_address="0xabcdefabcdefabcdefabcdefabcdefabcdefabcd"
response=$(make_request "eth_getCode" "[\"$contract_address\", \"latest\"]")
echo "Response:"
format_json "$response"
echo

# Test 10: Get storage at position
echo -e "${YELLOW}10. Getting storage at position...${NC}"
response=$(make_request "eth_getStorageAt" "[\"$contract_address\", \"0x0\", \"latest\"]")
echo "Response:"
format_json "$response"
echo

# Test 11: Call contract function
echo -e "${YELLOW}11. Calling contract function...${NC}"
call_object="{\"to\":\"$contract_address\",\"data\":\"0x6d4ce63c\"}"
response=$(make_request "eth_call" "[$call_object, \"latest\"]")
echo "Response:"
format_json "$response"
echo

# Test 12: Estimate gas
echo -e "${YELLOW}12. Estimating gas for transaction...${NC}"
response=$(make_request "eth_estimateGas" "[$call_object]")
echo "Response:"
format_json "$response"
echo

# Test 13: Get transaction by hash (example)
echo -e "${YELLOW}13. Getting transaction by hash...${NC}"
tx_hash="0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"
response=$(make_request "eth_getTransactionByHash" "[\"$tx_hash\"]")
echo "Response:"
format_json "$response"
echo

# Test 14: Get transaction receipt (example)
echo -e "${YELLOW}14. Getting transaction receipt...${NC}"
response=$(make_request "eth_getTransactionReceipt" "[\"$tx_hash\"]")
echo "Response:"
format_json "$response"
echo

# Test 15: Send raw transaction (example - will fail without valid transaction)
echo -e "${YELLOW}15. Sending raw transaction (example)...${NC}"
raw_tx="0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"
response=$(make_request "eth_sendRawTransaction" "[\"$raw_tx\"]")
echo "Response:"
format_json "$response"
echo

echo -e "${GREEN}=== All API examples completed ===${NC}"

# Additional utility functions
echo -e "\n${BLUE}=== Utility Functions ===${NC}"

# Function to convert hex to decimal
hex_to_decimal() {
    local hex="$1"
    if [[ $hex == 0x* ]]; then
        printf "%d\n" "$hex"
    else
        printf "%d\n" "0x$hex"
    fi
}

# Function to convert decimal to hex
decimal_to_hex() {
    local decimal="$1"
    printf "0x%x\n" "$decimal"
}

# Function to convert wei to ether
wei_to_ether() {
    local wei="$1"
    local ether=$(echo "scale=18; $wei / 1000000000000000000" | bc -l 2>/dev/null || echo "0")
    echo "$ether"
}

echo -e "${YELLOW}Utility functions available:${NC}"
echo "  hex_to_decimal <hex_value>     - Convert hex to decimal"
echo "  decimal_to_hex <decimal_value> - Convert decimal to hex"
echo "  wei_to_ether <wei_value>       - Convert wei to ether"

# Example usage of utility functions
echo -e "\n${YELLOW}Example utility usage:${NC}"
echo "hex_to_decimal 0x1a: $(hex_to_decimal 0x1a)"
echo "decimal_to_hex 26: $(decimal_to_hex 26)"
echo "wei_to_ether 1000000000000000000: $(wei_to_ether 1000000000000000000)"

echo -e "\n${GREEN}=== Script completed ===${NC}"
