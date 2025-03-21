#include "textbuffer/piece_tree_snapshot.h"

namespace textbuffer {

PieceTreeSnapshot::PieceTreeSnapshot(PieceTreeBase* tree, const std::string& BOM)
    : _index(0), _tree(tree), _BOM(BOM) {
    
    if (tree->root != SENTINEL) {
        tree->iterate(tree->root, [&](TreeNode* node) {
            if (node != SENTINEL) {
                _pieces.push_back(node->piece);
            }
            return true;
        });
    }
}

std::string PieceTreeSnapshot::read() {
    if (_pieces.empty()) {
        if (_index == 0) {
            _index++;
            return _BOM;
        } else {
            return "";
        }
    }

    if (_index > _pieces.size() - 1) {
        return "";
    }

    if (_index == 0) {
        return _BOM + _tree->getPieceContent(_pieces[_index++]);
    }
    
    return _tree->getPieceContent(_pieces[_index++]);
}

} // namespace textbuffer 