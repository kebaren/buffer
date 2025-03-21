#pragma once

#include <cstdint>
#include <memory>

namespace textbuffer {

// Forward declarations
class Piece;
class PieceTreeBase;

/**
 * Node color for the Red-Black tree
 */
enum class NodeColor : uint8_t {
    Black = 0,
    Red = 1
};

/**
 * Tree node in the RB tree
 */
class TreeNode {
public:
    TreeNode* parent;
    TreeNode* left;
    TreeNode* right;
    NodeColor color;

    // Piece data
    Piece* piece;
    int32_t size_left;  // size of the left subtree (not inorder)
    int32_t lf_left;    // line feeds count in the left subtree (not in order)

    /**
     * Create a new tree node
     */
    TreeNode(Piece* piece, NodeColor color);

    /**
     * Get the next node in order
     */
    TreeNode* next();

    /**
     * Get the previous node in order
     */
    TreeNode* prev();

    /**
     * Detach this node (set parent, left, right to null)
     */
    void detach();
};

/**
 * Sentinel node for the RB tree (null node)
 */
extern TreeNode* SENTINEL;

/**
 * Initialize the SENTINEL node
 */
void initializeSentinel();

/**
 * Get the leftmost node in a subtree
 */
TreeNode* leftest(TreeNode* node);

/**
 * Get the rightmost node in a subtree
 */
TreeNode* righttest(TreeNode* node);

/**
 * Calculate the size of a subtree
 */
int32_t calculateSize(TreeNode* node);

/**
 * Calculate the line feed count of a subtree
 */
int32_t calculateLF(TreeNode* node);

/**
 * Reset the SENTINEL node parent
 */
void resetSentinel();

/**
 * Left rotate a node in the RB tree
 */
void leftRotate(PieceTreeBase* tree, TreeNode* x);

/**
 * Right rotate a node in the RB tree
 */
void rightRotate(PieceTreeBase* tree, TreeNode* y);

/**
 * Delete a node from the RB tree
 */
void rbDelete(PieceTreeBase* tree, TreeNode* z);

/**
 * Fix the RB tree after insertion
 */
void fixInsert(PieceTreeBase* tree, TreeNode* x);

/**
 * Update the tree metadata (size_left, lf_left) for a node and its ancestors
 */
void updateTreeMetadata(PieceTreeBase* tree, TreeNode* x, int32_t delta, int32_t lineFeedCntDelta);

/**
 * Recompute the tree metadata for a node
 */
void recomputeTreeMetadata(PieceTreeBase* tree, TreeNode* x);

} // namespace textbuffer 