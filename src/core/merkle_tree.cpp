/**
 * @file merkle_tree.cpp
 * @brief Merkle tree implementation for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "core/merkle_tree.h"
#include "crypto/hash.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace deo {
namespace core {

MerkleTree::MerkleTree(const std::vector<std::string>& hashes) {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Creating Merkle tree with " + std::to_string(hashes.size()) + " leaves");
    
    leaf_hashes_ = hashes;
    height_ = calculateHeight(hashes.size());
    
    if (!hashes.empty()) {
        root_ = buildTreeRecursive(hashes);
    }
}

std::string MerkleTree::getRoot() const {
    if (root_) {
        return root_->hash;
    }
    // Empty Merkle tree root (Bitcoin convention)
    return "0000000000000000000000000000000000000000000000000000000000000000";
}

size_t MerkleTree::getHeight() const {
    return height_;
}

size_t MerkleTree::getLeafCount() const {
    return leaf_hashes_.size();
}

void MerkleTree::buildTree(const std::vector<std::string>& hashes) {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Building Merkle tree with " + std::to_string(hashes.size()) + " leaves");
    
    leaf_hashes_ = hashes;
    height_ = calculateHeight(hashes.size());
    
    if (!hashes.empty()) {
        root_ = buildTreeRecursive(hashes);
    } else {
        root_ = nullptr;
    }
}

void MerkleTree::addHash(const std::string& hash) {
    if (hash.empty()) {
        DEO_ERROR(VALIDATION, "Cannot add empty hash to Merkle tree");
        return;
    }
    
    leaf_hashes_.push_back(hash);
    height_ = calculateHeight(leaf_hashes_.size());
    
    // Rebuild the tree
    if (!leaf_hashes_.empty()) {
        root_ = buildTreeRecursive(leaf_hashes_);
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Added hash to Merkle tree, new leaf count: " + std::to_string(leaf_hashes_.size()));
}

bool MerkleTree::removeHash(const std::string& hash) {
    auto it = std::find(leaf_hashes_.begin(), leaf_hashes_.end(), hash);
    if (it != leaf_hashes_.end()) {
        leaf_hashes_.erase(it);
        height_ = calculateHeight(leaf_hashes_.size());
        
        // Rebuild the tree
        if (!leaf_hashes_.empty()) {
            root_ = buildTreeRecursive(leaf_hashes_);
        } else {
            root_ = nullptr;
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Removed hash from Merkle tree, new leaf count: " + std::to_string(leaf_hashes_.size()));
        return true;
    }
    
    return false;
}

bool MerkleTree::containsHash(const std::string& hash) const {
    return std::find(leaf_hashes_.begin(), leaf_hashes_.end(), hash) != leaf_hashes_.end();
}

MerkleProof MerkleTree::generateProof(const std::string& leaf_hash) const {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Generating Merkle proof for leaf: " + leaf_hash);
    
    if (!root_) {
        DEO_ERROR(VALIDATION, "Cannot generate proof from empty tree");
        return MerkleProof();
    }
    
    auto leaf_node = findLeaf(leaf_hash);
    if (!leaf_node) {
        DEO_ERROR(VALIDATION, "Leaf hash not found in tree");
        return MerkleProof();
    }
    
    return generateProofFromLeaf(leaf_node);
}

bool MerkleTree::verifyProof(const MerkleProof& proof, const std::string& root_hash) {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Verifying Merkle proof");
    
    if (proof.path.empty() || proof.directions.empty()) {
        DEO_ERROR(VALIDATION, "Empty proof path");
        return false;
    }
    
    if (proof.path.size() != proof.directions.size()) {
        DEO_ERROR(VALIDATION, "Proof path and directions size mismatch");
        return false;
    }
    
    std::string current_hash = proof.leaf_hash;
    
    // Traverse the proof path
    for (size_t i = 0; i < proof.path.size(); ++i) {
        const std::string& sibling_hash = proof.path[i];
        bool is_right = proof.directions[i];
        
        if (is_right) {
            // Current hash is left child, sibling is right child
            current_hash = hashChildren(current_hash, sibling_hash);
        } else {
            // Current hash is right child, sibling is left child
            current_hash = hashChildren(sibling_hash, current_hash);
        }
    }
    
    bool is_valid = (current_hash == root_hash);
    
    if (is_valid) {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Merkle proof verification successful");
    } else {
        DEO_ERROR(VALIDATION, "Merkle proof verification failed");
    }
    
    return is_valid;
}

bool MerkleTree::verifyInclusion(const std::string& hash) const {
    if (!root_) {
        return false;
    }
    
    MerkleProof proof = generateProof(hash);
    if (proof.path.empty()) {
        return false;
    }
    
    return verifyProof(proof, root_->hash);
}

std::vector<std::string> MerkleTree::getLeafHashes() const {
    return leaf_hashes_;
}

void MerkleTree::clear() {
    root_ = nullptr;
    leaf_hashes_.clear();
    height_ = 0;
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Merkle tree cleared");
}

bool MerkleTree::isEmpty() const {
    return !root_ || leaf_hashes_.empty();
}

std::string MerkleTree::getStatistics() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"leaf_count\": " << leaf_hashes_.size() << ",\n";
    ss << "  \"height\": " << height_ << ",\n";
    ss << "  \"root_hash\": \"" << getRoot() << "\",\n";
    ss << "  \"is_empty\": " << (isEmpty() ? "true" : "false") << "\n";
    ss << "}";
    
    return ss.str();
}

size_t MerkleTree::calculateHeight(size_t leaf_count) {
    if (leaf_count == 0) {
        return 0;
    }
    
    if (leaf_count == 1) {
        return 1;
    }
    
    // Calculate height: ceil(log2(leaf_count))
    size_t height = 1;
    size_t nodes = 1;
    
    while (nodes < leaf_count) {
        nodes *= 2;
        height++;
    }
    
    return height;
}

std::shared_ptr<MerkleNode> MerkleTree::buildTreeRecursive(const std::vector<std::string>& hashes) {
    if (hashes.empty()) {
        return nullptr;
    }
    
    if (hashes.size() == 1) {
        // Single leaf node
        auto node = std::make_shared<MerkleNode>(hashes[0], true);
        return node;
    }
    
    // Pad to even number of hashes
    std::vector<std::string> padded_hashes = hashes;
    if (padded_hashes.size() % 2 == 1) {
        padded_hashes.push_back(hashes.back()); // Duplicate last hash
    }
    
    // Create parent nodes
    std::vector<std::string> parent_hashes;
    for (size_t i = 0; i < padded_hashes.size(); i += 2) {
        std::string combined_hash = hashChildren(padded_hashes[i], padded_hashes[i + 1]);
        parent_hashes.push_back(combined_hash);
    }
    
    // Recursively build parent level
    auto parent_node = buildTreeRecursive(parent_hashes);
    
    // Create current level node
    auto node = std::make_shared<MerkleNode>(parent_node->hash, false);
    node->left = parent_node;
    
    return node;
}

std::string MerkleTree::hashChildren(const std::string& left_hash, const std::string& right_hash) {
    // Combine hashes and compute SHA-256
    std::string combined = left_hash + right_hash;
    return crypto::Hash::sha256(combined);
}

std::shared_ptr<MerkleNode> MerkleTree::findLeaf(const std::string& hash) const {
    if (!root_) {
        return nullptr;
    }
    
    // Search through leaf nodes
    auto leaf_nodes = getLeafNodes();
    for (auto& leaf : leaf_nodes) {
        if (leaf && leaf->hash == hash) {
            return leaf;
        }
    }
    
    return nullptr;
}

MerkleProof MerkleTree::generateProofFromLeaf(std::shared_ptr<MerkleNode> leaf_node) const {
    MerkleProof proof;
    proof.leaf_hash = leaf_node->hash;
    
    // Traverse up the tree to build the proof path
    std::shared_ptr<MerkleNode> current = leaf_node;
    std::shared_ptr<MerkleNode> parent = nullptr;
    
    // Find parent by searching the tree
    // Note: In a real implementation, we would maintain parent pointers
    // For now, we'll use a simplified approach
    
    // This is a placeholder implementation
    // In a real implementation, we would traverse up the tree properly
    DEO_LOG_WARNING(BLOCKCHAIN, "Merkle proof generation not fully implemented yet");
    
    return proof;
}

std::vector<std::shared_ptr<MerkleNode>> MerkleTree::getLeafNodes() const {
    std::vector<std::shared_ptr<MerkleNode>> leaves;
    if (root_) {
        collectLeaves(root_, leaves);
    }
    return leaves;
}

void MerkleTree::collectLeaves(std::shared_ptr<MerkleNode> node, 
                              std::vector<std::shared_ptr<MerkleNode>>& leaves) const {
    if (!node) {
        return;
    }
    
    if (node->is_leaf) {
        leaves.push_back(node);
    } else {
        collectLeaves(node->left, leaves);
        collectLeaves(node->right, leaves);
    }
}

} // namespace core
} // namespace deo