#pragma once

#include "piece_tree_base.h"
#include <vector>
#include <string>

namespace textbuffer {

/**
 * Readonly snapshot for piece tree.
 * In a real multiple thread environment, to make snapshot reading always work correctly, we need to
 * 1. Make TreeNode.piece immutable, then reading and writing can run in parallel.
 * 2. TreeNode/Buffers normalization should not happen during snapshot reading.
 */
class PieceTreeSnapshot : public ITextSnapshot {
private:
    std::vector<Piece*> _pieces;
    size_t _index;
    PieceTreeBase* _tree;
    std::string _BOM;

public:
    PieceTreeSnapshot(PieceTreeBase* tree, const std::string& BOM = "");
    
    /**
     * Read the next chunk from the snapshot
     */
    std::string read() override;
};

} // namespace textbuffer 