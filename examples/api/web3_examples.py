#!/usr/bin/env python3
"""
Deo Blockchain Web3 API Examples

This script demonstrates how to interact with the Deo blockchain
using the Web3-compatible JSON-RPC API.

Author: Deo Blockchain Research Team
License: MIT
"""

import requests
import json
import time
from typing import Dict, Any, Optional


class DeoWeb3Client:
    """Client for interacting with Deo blockchain via Web3-compatible API"""
    
    def __init__(self, rpc_url: str = "http://localhost:8545"):
        self.rpc_url = rpc_url
        self.request_id = 1
    
    def _make_request(self, method: str, params: list = None) -> Dict[str, Any]:
        """Make a JSON-RPC request to the Deo node"""
        if params is None:
            params = []
            
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": self.request_id
        }
        
        self.request_id += 1
        
        try:
            response = requests.post(
                self.rpc_url,
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=30
            )
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            return {"error": f"Request failed: {e}"}
    
    def get_block_number(self) -> Optional[str]:
        """Get the current block number"""
        result = self._make_request("eth_blockNumber")
        return result.get("result")
    
    def get_balance(self, address: str, block_tag: str = "latest") -> Optional[str]:
        """Get the balance of an address"""
        result = self._make_request("eth_getBalance", [address, block_tag])
        return result.get("result")
    
    def get_block_by_number(self, block_number: str, include_transactions: bool = False) -> Optional[Dict]:
        """Get block information by block number"""
        result = self._make_request("eth_getBlockByNumber", [block_number, include_transactions])
        return result.get("result")
    
    def get_block_by_hash(self, block_hash: str, include_transactions: bool = False) -> Optional[Dict]:
        """Get block information by block hash"""
        result = self._make_request("eth_getBlockByHash", [block_hash, include_transactions])
        return result.get("result")
    
    def get_transaction_by_hash(self, tx_hash: str) -> Optional[Dict]:
        """Get transaction information by hash"""
        result = self._make_request("eth_getTransactionByHash", [tx_hash])
        return result.get("result")
    
    def get_transaction_receipt(self, tx_hash: str) -> Optional[Dict]:
        """Get transaction receipt by hash"""
        result = self._make_request("eth_getTransactionReceipt", [tx_hash])
        return result.get("result")
    
    def call_contract(self, call_object: Dict, block_tag: str = "latest") -> Optional[str]:
        """Call a contract function"""
        result = self._make_request("eth_call", [call_object, block_tag])
        return result.get("result")
    
    def send_raw_transaction(self, raw_tx: str) -> Optional[str]:
        """Send a raw transaction"""
        result = self._make_request("eth_sendRawTransaction", [raw_tx])
        return result.get("result")
    
    def estimate_gas(self, call_object: Dict) -> Optional[str]:
        """Estimate gas for a transaction"""
        result = self._make_request("eth_estimateGas", [call_object])
        return result.get("result")
    
    def get_gas_price(self) -> Optional[str]:
        """Get current gas price"""
        result = self._make_request("eth_gasPrice")
        return result.get("result")
    
    def get_code(self, address: str, block_tag: str = "latest") -> Optional[str]:
        """Get contract code at an address"""
        result = self._make_request("eth_getCode", [address, block_tag])
        return result.get("result")
    
    def get_storage_at(self, address: str, position: str, block_tag: str = "latest") -> Optional[str]:
        """Get storage value at a specific position"""
        result = self._make_request("eth_getStorageAt", [address, position, block_tag])
        return result.get("result")
    
    def get_network_version(self) -> Optional[str]:
        """Get network version"""
        result = self._make_request("net_version")
        return result.get("result")
    
    def is_listening(self) -> Optional[bool]:
        """Check if node is listening"""
        result = self._make_request("net_listening")
        return result.get("result")
    
    def get_peer_count(self) -> Optional[str]:
        """Get number of connected peers"""
        result = self._make_request("net_peerCount")
        return result.get("result")
    
    def get_client_version(self) -> Optional[str]:
        """Get client version"""
        result = self._make_request("web3_clientVersion")
        return result.get("result")


def hex_to_int(hex_str: str) -> int:
    """Convert hex string to integer"""
    if hex_str.startswith("0x"):
        return int(hex_str, 16)
    return int(hex_str)


def int_to_hex(value: int) -> str:
    """Convert integer to hex string"""
    return hex(value)


def wei_to_ether(wei: str) -> float:
    """Convert wei to ether"""
    return hex_to_int(wei) / 10**18


def demonstrate_basic_operations():
    """Demonstrate basic blockchain operations"""
    print("=== Deo Blockchain Web3 API Demonstration ===\n")
    
    # Initialize client
    client = DeoWeb3Client()
    
    # Test connection
    print("1. Testing connection...")
    version = client.get_client_version()
    if version:
        print(f"   Connected to: {version}")
    else:
        print("   Failed to connect to Deo node")
        return
    
    # Get network information
    print("\n2. Network Information:")
    network_version = client.get_network_version()
    is_listening = client.is_listening()
    peer_count = client.get_peer_count()
    
    print(f"   Network Version: {network_version}")
    print(f"   Node Listening: {is_listening}")
    print(f"   Peer Count: {hex_to_int(peer_count) if peer_count else 0}")
    
    # Get current block information
    print("\n3. Current Block Information:")
    block_number = client.get_block_number()
    if block_number:
        current_block = hex_to_int(block_number)
        print(f"   Current Block: {current_block}")
        
        # Get block details
        block_info = client.get_block_by_number(block_number, False)
        if block_info:
            print(f"   Block Hash: {block_info.get('hash', 'N/A')}")
            print(f"   Parent Hash: {block_info.get('parentHash', 'N/A')}")
            print(f"   Timestamp: {hex_to_int(block_info.get('timestamp', '0x0'))}")
    else:
        print("   Failed to get block number")
    
    # Get gas information
    print("\n4. Gas Information:")
    gas_price = client.get_gas_price()
    if gas_price:
        gas_price_wei = hex_to_int(gas_price)
        gas_price_gwei = gas_price_wei / 10**9
        print(f"   Gas Price: {gas_price_wei} wei ({gas_price_gwei:.2f} gwei)")
    
    # Demonstrate account operations
    print("\n5. Account Operations:")
    test_address = "0x1234567890123456789012345678901234567890"
    balance = client.get_balance(test_address)
    if balance:
        balance_wei = hex_to_int(balance)
        balance_ether = wei_to_ether(balance)
        print(f"   Balance of {test_address}: {balance_wei} wei ({balance_ether:.6f} ether)")
    
    # Demonstrate contract operations
    print("\n6. Contract Operations:")
    contract_address = "0xabcdefabcdefabcdefabcdefabcdefabcdefabcd"
    
    # Get contract code
    code = client.get_code(contract_address)
    if code:
        print(f"   Contract code at {contract_address}: {code[:50]}...")
    else:
        print(f"   No contract code at {contract_address}")
    
    # Get storage value
    storage_value = client.get_storage_at(contract_address, "0x0")
    if storage_value:
        print(f"   Storage at position 0x0: {storage_value}")
    
    print("\n=== Demonstration Complete ===")


def demonstrate_contract_interaction():
    """Demonstrate contract interaction"""
    print("\n=== Contract Interaction Demonstration ===\n")
    
    client = DeoWeb3Client()
    
    # Example contract call
    contract_address = "0x1234567890123456789012345678901234567890"
    
    # Call a contract function (example: get() function)
    call_object = {
        "to": contract_address,
        "data": "0x6d4ce63c"  # Function selector for get()
    }
    
    print("1. Calling contract function...")
    result = client.call_contract(call_object)
    if result:
        print(f"   Contract call result: {result}")
    else:
        print("   Contract call failed")
    
    # Estimate gas for a transaction
    print("\n2. Estimating gas...")
    gas_estimate = client.estimate_gas(call_object)
    if gas_estimate:
        gas_estimate_int = hex_to_int(gas_estimate)
        print(f"   Estimated gas: {gas_estimate_int}")
    
    print("\n=== Contract Interaction Complete ===")


def monitor_blockchain():
    """Monitor blockchain for new blocks"""
    print("\n=== Blockchain Monitoring ===\n")
    
    client = DeoWeb3Client()
    
    print("Monitoring for new blocks (press Ctrl+C to stop)...")
    
    last_block = None
    try:
        while True:
            current_block = client.get_block_number()
            if current_block and current_block != last_block:
                block_num = hex_to_int(current_block)
                print(f"New block: {block_num}")
                
                # Get block details
                block_info = client.get_block_by_number(current_block, False)
                if block_info:
                    timestamp = hex_to_int(block_info.get('timestamp', '0x0'))
                    print(f"  Timestamp: {timestamp}")
                    print(f"  Hash: {block_info.get('hash', 'N/A')}")
                
                last_block = current_block
            
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nMonitoring stopped.")


if __name__ == "__main__":
    # Run demonstrations
    demonstrate_basic_operations()
    demonstrate_contract_interaction()
    
    # Uncomment to enable monitoring
    # monitor_blockchain()
