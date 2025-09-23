/**
 * @file merkle_tree.h
 * @brief Merkle tree implementation for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace deo {
namespace core {

/**
 * @brief Merkle tree node structure
 */
struct MerkleNode {
    std::string hash;                    ///< Node hash
    std::shared_ptr<MerkleNode> left;    ///< Left child node
    std::shared_ptr<MerkleNode> right;   ///< Right child node
    bool is_leaf;                        ///< Whether this is a leaf node
    
    /**
     * @brief Constructor
     * @param h Node hash
     * @param leaf Whether this is a leaf node
     */
    MerkleNode(const std::string& h, bool leaf = false)
        : hash(h), is_leaf(leaf) {}
};

/**
 * @brief Merkle proof structure for inclusion verification
 */
struct MerkleProof {
    std::string leaf_hash;               ///< Hash of the leaf being proven
    std::vector<std::string> path;       ///< Path from leaf to root
    std::vector<bool> directions;        ///< Direction for each step (true = right, false = left)
    
    /**
     * @brief Default constructor
     */
    MerkleProof() = default;
    
    /**
     * @brief Constructor with parameters
     * @param leaf Hash of the leaf
     * @param p Path hashes
     * @param dir Directions
     */
    MerkleProof(const std::string& leaf, 
                const std::vector<std::string>& p,
                const std::vector<bool>& dir)
        : leaf_hash(leaf), path(p), directions(dir) {}
};

/**
 * @brief Merkle tree implementation using SHA-256
 * 
 * A Merkle tree is a binary tree where each leaf node contains the hash
 * of a data block, and each non-leaf node contains the hash of its children.
 * This provides an efficient way to verify the integrity of large datasets.
 */
class MerkleTree {
public:
    /**
     * @brief Constructor with leaf hashes
     * @param hashes Vector of leaf hashes
     */
    explicit MerkleTree(const std::vector<std::string>& hashes);
    
    /**
     * @brief Default constructor
     */
    MerkleTree() = default;
    
    /**
     * @brief Destructor
     */
    ~MerkleTree() = default;
    
    // Disable copy and move semantics
    MerkleTree(const MerkleTree&) = delete;
    MerkleTree& operator=(const MerkleTree&) = delete;
    MerkleTree(MerkleTree&&) = delete;
    MerkleTree& operator=(MerkleTree&&) = delete;
    
    /**
     * @brief Get the Merkle root hash
     * @return Root hash of the tree
     */
    std::string getRoot() const;
    
    /**
     * @brief Get the height of the tree
     * @return Tree height
     */
    size_t getHeight() const;
    
    /**
     * @brief Get the number of leaf nodes
     * @return Number of leaves
     */
    size_t getLeafCount() const;
    
    /**
     * @brief Build the tree from leaf hashes
     * @param hashes Vector of leaf hashes
     */
    void buildTree(const std::vector<std::string>& hashes);
    
    /**
     * @brief Add a hash to the tree
     * @param hash Hash to add
     */
    void addHash(const std::string& hash);
    
    /**
     * @brief Remove a hash from the tree
     * @param hash Hash to remove
     * @return True if hash was found and removed
     */
    bool removeHash(const std::string& hash);
    
    /**
     * @brief Check if a hash exists in the tree
     * @param hash Hash to check
     * @return True if hash exists
     */
    bool containsHash(const std::string& hash) const;
    
    /**
     * @brief Generate a Merkle proof for a leaf
     * @param leaf_hash Hash of the leaf to prove
     * @return Merkle proof or empty proof if not found
     */
    MerkleProof generateProof(const std::string& leaf_hash) const;
    
    /**
     * @brief Verify a Merkle proof
     * @param proof Merkle proof to verify
     * @param root_hash Expected root hash
     * @return True if proof is valid
     */
    static bool verifyProof(const MerkleProof& proof, const std::string& root_hash);
    
    /**
     * @brief Verify inclusion of a hash in the tree
     * @param hash Hash to verify
     * @return True if hash is included
     */
    bool verifyInclusion(const std::string& hash) const;
    
    /**
     * @brief Get all leaf hashes
     * @return Vector of leaf hashes
     */
    std::vector<std::string> getLeafHashes() const;
    
    /**
     * @brief Clear the tree
     */
    void clear();
    
    /**
     * @brief Check if tree is empty
     * @return True if tree is empty
     */
    bool isEmpty() const;
    
    /**
     * @brief Get tree statistics
     * @return JSON string with tree statistics
     */
    std::string getStatistics() const;

private:
    std::shared_ptr<MerkleNode> root_;           ///< Root node of the tree
    std::vector<std::string> leaf_hashes_;       ///< Original leaf hashes
    size_t height_;                              ///< Tree height
    
    /**
     * @brief Calculate the height of the tree based on leaf count
     * @param leaf_count Number of leaves
     * @return Tree height
     */
    static size_t calculateHeight(size_t leaf_count);
    
    /**
     * @brief Build tree recursively
     * @param hashes Vector of hashes to build from
     * @return Root node of the subtree
     */
    std::shared_ptr<MerkleNode> buildTreeRecursive(const std::vector<std::string>& hashes);
    
    /**
     * @brief Hash two child nodes
     * @param left_hash Left child hash
     * @param right_hash Right child hash
     * @return Combined hash
     */
    static std::string hashChildren(const std::string& left_hash, const std::string& right_hash);
    
    /**
     * @brief Find a leaf node by hash
     * @param hash Hash to find
     * @return Pointer to leaf node or nullptr if not found
     */
    std::shared_ptr<MerkleNode> findLeaf(const std::string& hash) const;
    
    /**
     * @brief Generate proof path from leaf to root
     * @param leaf_node Leaf node to start from
     * @return Merkle proof
     */
    MerkleProof generateProofFromLeaf(std::shared_ptr<MerkleNode> leaf_node) const;
    
    /**
     * @brief Get all leaf nodes
     * @return Vector of leaf node pointers
     */
    std::vector<std::shared_ptr<MerkleNode>> getLeafNodes() const;
    
    /**
     * @brief Collect leaf nodes recursively
     * @param node Current node
     * @param leaves Vector to collect leaves in
     */
    void collectLeaves(std::shared_ptr<MerkleNode> node, 
                      std::vector<std::shared_ptr<MerkleNode>>& leaves) const;
};

} // namespace core
} // namespace deo