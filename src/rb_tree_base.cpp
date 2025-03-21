#include "textbuffer/rb_tree_base.h"
#include "textbuffer/piece_tree_base.h"

namespace textbuffer {

// Initialize the sentinel node
static TreeNode _SENTINEL_INSTANCE(nullptr, NodeColor::Black);
TreeNode* SENTINEL = &_SENTINEL_INSTANCE;

void initializeSentinel() {
    SENTINEL->parent = SENTINEL;
    SENTINEL->left = SENTINEL;
    SENTINEL->right = SENTINEL;
    SENTINEL->color = NodeColor::Black;
    SENTINEL->size_left = 0;
    SENTINEL->lf_left = 0;
}

TreeNode::TreeNode(Piece* piece, NodeColor color) : piece(piece), color(color) {
    size_left = 0;
    lf_left = 0;
    parent = this;
    left = this;
    right = this;
}

TreeNode* TreeNode::next() {
    if (right != SENTINEL) {
        return leftest(right);
    }

    TreeNode* node = this;

    while (node->parent != SENTINEL) {
        if (node->parent->left == node) {
            break;
        }

        node = node->parent;
    }

    if (node->parent == SENTINEL) {
        return SENTINEL;
    } else {
        return node->parent;
    }
}

TreeNode* TreeNode::prev() {
    if (left != SENTINEL) {
        return righttest(left);
    }

    TreeNode* node = this;

    while (node->parent != SENTINEL) {
        if (node->parent->right == node) {
            break;
        }

        node = node->parent;
    }

    if (node->parent == SENTINEL) {
        return SENTINEL;
    } else {
        return node->parent;
    }
}

void TreeNode::detach() {
    parent = nullptr;
    left = nullptr;
    right = nullptr;
}

TreeNode* leftest(TreeNode* node) {
    while (node->left != SENTINEL) {
        node = node->left;
    }
    return node;
}

TreeNode* righttest(TreeNode* node) {
    while (node->right != SENTINEL) {
        node = node->right;
    }
    return node;
}

int32_t calculateSize(TreeNode* node) {
    if (node == SENTINEL) {
        return 0;
    }
    return node->size_left + node->piece->length + calculateSize(node->right);
}

int32_t calculateLF(TreeNode* node) {
    if (node == SENTINEL) {
        return 0;
    }
    return node->lf_left + node->piece->lineFeedCnt + calculateLF(node->right);
}

void resetSentinel() {
    SENTINEL->parent = SENTINEL;
}

void leftRotate(PieceTreeBase* tree, TreeNode* x) {
    if (!x || x == SENTINEL || x->right == SENTINEL) {
        return;  // 防御性检查
    }

    TreeNode* y = x->right;

    // 保存旧的元数据值用于后续更新
    int32_t oldSizeLeftX = x->size_left;
    int32_t oldLFLeftX = x->lf_left;
    int32_t nodeSizeX = (x->piece ? x->piece->length : 0);
    int32_t nodeLFX = (x->piece ? x->piece->lineFeedCnt : 0);
    
    // 执行旋转
    x->right = y->left;
    if (y->left != SENTINEL) {
        y->left->parent = x;
    }
    
    y->parent = x->parent;
    if (x->parent == SENTINEL) {
        tree->root = y;
    } else if (x->parent->left == x) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    
    y->left = x;
    x->parent = y;
    
    // 更新元数据
    // 旋转后，y节点左侧的size和lf需要包含x和x的左子树
    y->size_left = oldSizeLeftX + nodeSizeX;
    y->lf_left = oldLFLeftX + nodeLFX;
    
    // 重新计算x的子树元数据
    recomputeTreeMetadata(tree, x);
}

void rightRotate(PieceTreeBase* tree, TreeNode* y) {
    if (!y || y == SENTINEL || y->left == SENTINEL) {
        return;  // 防御性检查
    }

    TreeNode* x = y->left;
    
    // 保存旧的元数据值用于后续更新
    int32_t oldSizeLeftY = y->size_left;
    int32_t oldLFLeftY = y->lf_left;
    int32_t nodeSizeX = (x->piece ? x->piece->length : 0);
    int32_t nodeLFX = (x->piece ? x->piece->lineFeedCnt : 0);
    int32_t rightTreeSizeX = calculateSize(x->right);
    int32_t rightTreeLFX = calculateLF(x->right);
    
    // 执行旋转
    y->left = x->right;
    if (x->right != SENTINEL) {
        x->right->parent = y;
    }
    
    x->parent = y->parent;
    if (y->parent == SENTINEL) {
        tree->root = x;
    } else if (y->parent->right == y) {
        y->parent->right = x;
    } else {
        y->parent->left = x;
    }
    
    x->right = y;
    y->parent = x;
    
    // 更新元数据
    // 旋转后，y的左子树只包含x的右子树
    y->size_left = rightTreeSizeX;
    y->lf_left = rightTreeLFX;
    
    // 重新计算x的子树元数据
    recomputeTreeMetadata(tree, y);
    recomputeTreeMetadata(tree, x);
}

void rbDelete(PieceTreeBase* tree, TreeNode* z) {
    TreeNode* x;
    TreeNode* y;

    if (z->left == SENTINEL) {
        y = z;
        x = y->right;
    } else if (z->right == SENTINEL) {
        y = z;
        x = y->left;
    } else {
        y = leftest(z->right);
        x = y->right;
    }

    if (y == tree->root) {
        tree->root = x;

        // if x is null, we are removing the only node
        x->color = NodeColor::Black;
        z->detach();
        resetSentinel();
        tree->root->parent = SENTINEL;

        return;
    }

    bool yWasRed = (y->color == NodeColor::Red);

    if (y == y->parent->left) {
        y->parent->left = x;
    } else {
        y->parent->right = x;
    }

    if (y == z) {
        x->parent = y->parent;
        recomputeTreeMetadata(tree, x);
    } else {
        if (y->parent == z) {
            x->parent = y;
        } else {
            x->parent = y->parent;
        }

        // as we make changes to x's hierarchy, update size_left of subtree first
        recomputeTreeMetadata(tree, x);

        y->left = z->left;
        y->right = z->right;
        y->parent = z->parent;
        y->color = z->color;

        if (z == tree->root) {
            tree->root = y;
        } else {
            if (z == z->parent->left) {
                z->parent->left = y;
            } else {
                z->parent->right = y;
            }
        }

        if (y->left != SENTINEL) {
            y->left->parent = y;
        }
        if (y->right != SENTINEL) {
            y->right->parent = y;
        }
        // update metadata
        // we replace z with y, so in this sub tree, the length change is z.item.length
        y->size_left = z->size_left;
        y->lf_left = z->lf_left;
        recomputeTreeMetadata(tree, y);
    }

    z->detach();

    if (x->parent->left == x) {
        int32_t newSizeLeft = calculateSize(x);
        int32_t newLFLeft = calculateLF(x);
        if (newSizeLeft != x->parent->size_left || newLFLeft != x->parent->lf_left) {
            int32_t delta = newSizeLeft - x->parent->size_left;
            int32_t lf_delta = newLFLeft - x->parent->lf_left;
            x->parent->size_left = newSizeLeft;
            x->parent->lf_left = newLFLeft;
            updateTreeMetadata(tree, x->parent, delta, lf_delta);
        }
    }

    recomputeTreeMetadata(tree, x->parent);

    if (yWasRed) {
        resetSentinel();
        return;
    }

    // RB-DELETE-FIXUP
    TreeNode* w;
    while (x != tree->root && x->color == NodeColor::Black) {
        if (x == x->parent->left) {
            w = x->parent->right;

            if (w->color == NodeColor::Red) {
                w->color = NodeColor::Black;
                x->parent->color = NodeColor::Red;
                leftRotate(tree, x->parent);
                w = x->parent->right;
            }

            if (w->left->color == NodeColor::Black && w->right->color == NodeColor::Black) {
                w->color = NodeColor::Red;
                x = x->parent;
            } else {
                if (w->right->color == NodeColor::Black) {
                    w->left->color = NodeColor::Black;
                    w->color = NodeColor::Red;
                    rightRotate(tree, w);
                    w = x->parent->right;
                }

                w->color = x->parent->color;
                x->parent->color = NodeColor::Black;
                w->right->color = NodeColor::Black;
                leftRotate(tree, x->parent);
                x = tree->root;
            }
        } else {
            w = x->parent->left;

            if (w->color == NodeColor::Red) {
                w->color = NodeColor::Black;
                x->parent->color = NodeColor::Red;
                rightRotate(tree, x->parent);
                w = x->parent->left;
            }

            if (w->left->color == NodeColor::Black && w->right->color == NodeColor::Black) {
                w->color = NodeColor::Red;
                x = x->parent;
            } else {
                if (w->left->color == NodeColor::Black) {
                    w->right->color = NodeColor::Black;
                    w->color = NodeColor::Red;
                    leftRotate(tree, w);
                    w = x->parent->left;
                }

                w->color = x->parent->color;
                x->parent->color = NodeColor::Black;
                w->left->color = NodeColor::Black;
                rightRotate(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->color = NodeColor::Black;
    resetSentinel();
}

void fixInsert(PieceTreeBase* tree, TreeNode* x) {
    recomputeTreeMetadata(tree, x);

    while (x != tree->root && x->parent->color == NodeColor::Red) {
        if (x->parent == x->parent->parent->left) {
            TreeNode* y = x->parent->parent->right;

            if (y->color == NodeColor::Red) {
                x->parent->color = NodeColor::Black;
                y->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->right) {
                    x = x->parent;
                    leftRotate(tree, x);
                }

                x->parent->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                rightRotate(tree, x->parent->parent);
            }
        } else {
            TreeNode* y = x->parent->parent->left;

            if (y->color == NodeColor::Red) {
                x->parent->color = NodeColor::Black;
                y->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->left) {
                    x = x->parent;
                    rightRotate(tree, x);
                }
                x->parent->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                leftRotate(tree, x->parent->parent);
            }
        }
    }

    tree->root->color = NodeColor::Black;
}

void updateTreeMetadata(PieceTreeBase* tree, TreeNode* x, int32_t delta, int32_t lineFeedCntDelta) {
    // Walk up the tree and update metadata
    while (x != SENTINEL) {
        if (delta != 0) {
            x->size_left += delta;
        }
        
        if (lineFeedCntDelta != 0) {
            x->lf_left += lineFeedCntDelta;
        }
        
        x = x->parent;
    }
}

void recomputeTreeMetadata(PieceTreeBase* tree, TreeNode* x) {
    if (x == SENTINEL) {
        return;
    }
    
    x->size_left = (x->left != SENTINEL ? x->left->size_left + x->left->piece->length + calculateSize(x->left->right) : 0);
    x->lf_left = (x->left != SENTINEL ? x->left->lf_left + x->left->piece->lineFeedCnt + calculateLF(x->left->right) : 0);
    
    // Recursively recompute for the parent node
    recomputeTreeMetadata(tree, x->parent);
}

} // namespace textbuffer 