#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "position.h"

namespace textbuffer {
namespace common {

/**
 * A range in the editor. This interface is suitable for serialization.
 */
class IRange {
public:
    virtual ~IRange() = default;

    /**
     * Line number on which the range starts (starts at 1).
     */
    virtual int32_t startLineNumber() const = 0;

    /**
     * Column on which the range starts in line `startLineNumber` (starts at 1).
     */
    virtual int32_t startColumn() const = 0;

    /**
     * Line number on which the range ends.
     */
    virtual int32_t endLineNumber() const = 0;

    /**
     * Column on which the range ends in line `endLineNumber`.
     */
    virtual int32_t endColumn() const = 0;
};

/**
 * A range in the editor. (startLineNumber,startColumn) is <= (endLineNumber,endColumn)
 */
class Range : public IRange {
private:
    int32_t _startLineNumber;
    int32_t _startColumn;
    int32_t _endLineNumber;
    int32_t _endColumn;

public:
    Range(int32_t startLineNumber, int32_t startColumn, int32_t endLineNumber, int32_t endColumn) {
        if ((startLineNumber > endLineNumber) || (startLineNumber == endLineNumber && startColumn > endColumn)) {
            _startLineNumber = endLineNumber;
            _startColumn = endColumn;
            _endLineNumber = startLineNumber;
            _endColumn = startColumn;
        } else {
            _startLineNumber = startLineNumber;
            _startColumn = startColumn;
            _endLineNumber = endLineNumber;
            _endColumn = endColumn;
        }
    }

    /**
     * Line number on which the range starts (starts at 1).
     */
    int32_t startLineNumber() const override {
        return _startLineNumber;
    }

    /**
     * Column on which the range starts in line `startLineNumber` (starts at 1).
     */
    int32_t startColumn() const override {
        return _startColumn;
    }

    /**
     * Line number on which the range ends.
     */
    int32_t endLineNumber() const override {
        return _endLineNumber;
    }

    /**
     * Column on which the range ends in line `endLineNumber`.
     */
    int32_t endColumn() const override {
        return _endColumn;
    }

    /**
     * Test if this range is empty.
     */
    bool isEmpty() const {
        return Range::isEmpty(*this);
    }

    /**
     * Test if `range` is empty.
     */
    static bool isEmpty(const IRange& range) {
        return (range.startLineNumber() == range.endLineNumber() && range.startColumn() == range.endColumn());
    }

    /**
     * Test if position is in this range. If the position is at the edges, will return true.
     */
    bool containsPosition(const IPosition& position) const {
        return Range::containsPosition(*this, position);
    }

    /**
     * Test if `position` is in `range`. If the position is at the edges, will return true.
     */
    static bool containsPosition(const IRange& range, const IPosition& position) {
        if (position.lineNumber() < range.startLineNumber() || position.lineNumber() > range.endLineNumber()) {
            return false;
        }
        if (position.lineNumber() == range.startLineNumber() && position.column() < range.startColumn()) {
            return false;
        }
        if (position.lineNumber() == range.endLineNumber() && position.column() > range.endColumn()) {
            return false;
        }
        return true;
    }

    /**
     * Test if range is in this range. If the range is equal to this range, will return true.
     */
    bool containsRange(const IRange& range) const {
        return Range::containsRange(*this, range);
    }

    /**
     * Test if `otherRange` is in `range`. If the ranges are equal, will return true.
     */
    static bool containsRange(const IRange& range, const IRange& otherRange) {
        if (otherRange.startLineNumber() < range.startLineNumber() || otherRange.endLineNumber() < range.startLineNumber()) {
            return false;
        }
        if (otherRange.startLineNumber() > range.endLineNumber() || otherRange.endLineNumber() > range.endLineNumber()) {
            return false;
        }
        if (otherRange.startLineNumber() == range.startLineNumber() && otherRange.startColumn() < range.startColumn()) {
            return false;
        }
        if (otherRange.endLineNumber() == range.endLineNumber() && otherRange.endColumn() > range.endColumn()) {
            return false;
        }
        return true;
    }

    /**
     * A reunion of the two ranges.
     * The smallest position will be used as the start point, and the largest position as the end point.
     */
    Range plusRange(const IRange& range) const {
        return Range::plusRange(*this, range);
    }

    /**
     * A reunion of the two ranges.
     * The smallest position will be used as the start point, and the largest position as the end point.
     */
    static Range plusRange(const IRange& a, const IRange& b) {
        int32_t startLineNumber = 0;
        int32_t startColumn = 0;
        int32_t endLineNumber = 0;
        int32_t endColumn = 0;

        if (a.startLineNumber() < b.startLineNumber()) {
            startLineNumber = a.startLineNumber();
            startColumn = a.startColumn();
        } else if (a.startLineNumber() > b.startLineNumber()) {
            startLineNumber = b.startLineNumber();
            startColumn = b.startColumn();
        } else {
            startLineNumber = a.startLineNumber();
            startColumn = std::min(a.startColumn(), b.startColumn());
        }

        if (a.endLineNumber() < b.endLineNumber()) {
            endLineNumber = b.endLineNumber();
            endColumn = b.endColumn();
        } else if (a.endLineNumber() > b.endLineNumber()) {
            endLineNumber = a.endLineNumber();
            endColumn = a.endColumn();
        } else {
            endLineNumber = a.endLineNumber();
            endColumn = std::max(a.endColumn(), b.endColumn());
        }

        return Range(startLineNumber, startColumn, endLineNumber, endColumn);
    }

    /**
     * Compute the intersection between this range and the provided range.
     * If there is no intersection, returns null.
     */
    std::unique_ptr<Range> intersectRanges(const IRange& range) const {
        return Range::intersectRanges(*this, range);
    }

    /**
     * Compute the intersection between the two ranges.
     * If there is no intersection, returns null.
     */
    static std::unique_ptr<Range> intersectRanges(const IRange& a, const IRange& b) {
        int32_t resultStartLineNumber = std::max(a.startLineNumber(), b.startLineNumber());
        int32_t resultEndLineNumber = std::min(a.endLineNumber(), b.endLineNumber());

        if (resultStartLineNumber > resultEndLineNumber) {
            // There is no intersection
            return nullptr;
        }

        int32_t resultStartColumn = 0;
        int32_t resultEndColumn = 0;

        if (a.startLineNumber() == b.startLineNumber()) {
            resultStartColumn = std::max(a.startColumn(), b.startColumn());
        } else {
            resultStartColumn = a.startLineNumber() < b.startLineNumber() ? b.startColumn() : a.startColumn();
        }

        if (a.endLineNumber() == b.endLineNumber()) {
            resultEndColumn = std::min(a.endColumn(), b.endColumn());
        } else {
            resultEndColumn = a.endLineNumber() < b.endLineNumber() ? a.endColumn() : b.endColumn();
        }

        if (resultStartLineNumber == resultEndLineNumber && resultStartColumn > resultEndColumn) {
            // There is no intersection
            return nullptr;
        }

        return std::make_unique<Range>(resultStartLineNumber, resultStartColumn, resultEndLineNumber, resultEndColumn);
    }

    /**
     * Test if this range equals other range.
     */
    bool equalsRange(const IRange* other) const {
        return Range::equalsRange(this, other);
    }

    /**
     * Test if range `a` equals range `b`.
     */
    static bool equalsRange(const IRange* a, const IRange* b) {
        if (!a && !b) {
            return true;
        }
        if (!a || !b) {
            return false;
        }
        return (
            a->startLineNumber() == b->startLineNumber() &&
            a->startColumn() == b->startColumn() &&
            a->endLineNumber() == b->endLineNumber() &&
            a->endColumn() == b->endColumn()
        );
    }

    /**
     * Get the end position of this range.
     */
    Position getEndPosition() const {
        return Position(_endLineNumber, _endColumn);
    }

    /**
     * Get the start position of this range.
     */
    Position getStartPosition() const {
        return Position(_startLineNumber, _startColumn);
    }

    /**
     * Transform to a human-readable representation.
     */
    std::string toString() const {
        return "[" + std::to_string(_startLineNumber) + "," + std::to_string(_startColumn) + " -> " +
            std::to_string(_endLineNumber) + "," + std::to_string(_endColumn) + "]";
    }

    /**
     * Create a new range using this range's start position, and using endLineNumber and endColumn as the end position.
     */
    Range setEndPosition(int32_t endLineNumber, int32_t endColumn) const {
        return Range(_startLineNumber, _startColumn, endLineNumber, endColumn);
    }

    /**
     * Create a new range using this range's end position, and using startLineNumber and startColumn as the start position.
     */
    Range setStartPosition(int32_t startLineNumber, int32_t startColumn) const {
        return Range(startLineNumber, startColumn, _endLineNumber, _endColumn);
    }

    /**
     * Create a new empty range using this range's start position.
     */
    Range collapseToStart() const {
        return Range::collapseToStart(*this);
    }

    /**
     * Create a new empty range using provided range's start position.
     */
    static Range collapseToStart(const IRange& range) {
        return Range(range.startLineNumber(), range.startColumn(), range.startLineNumber(), range.startColumn());
    }

    /**
     * Create a range from two positions.
     * If start > end, the positions will be swapped.
     */
    static Range fromPositions(const IPosition& start, const IPosition* end = nullptr) {
        if (!end) {
            return Range(start.lineNumber(), start.column(), start.lineNumber(), start.column());
        }
        return Range(start.lineNumber(), start.column(), end->lineNumber(), end->column());
    }

    /**
     * Create a `Range` from an `IRange`.
     */
    static std::unique_ptr<Range> lift(const IRange* range) {
        if (!range) {
            return nullptr;
        }
        return std::make_unique<Range>(range->startLineNumber(), range->startColumn(), range->endLineNumber(), range->endColumn());
    }

    /**
     * Test if two ranges are intersecting. If the ranges are touching it is not considered an intersection.
     */
    static bool areIntersecting(const IRange& a, const IRange& b) {
        // Check if `a` is before `b`
        if (a.endLineNumber() < b.startLineNumber() || (a.endLineNumber() == b.startLineNumber() && a.endColumn() < b.startColumn())) {
            return false;
        }

        // Check if `b` is before `a`
        if (b.endLineNumber() < a.startLineNumber() || (b.endLineNumber() == a.startLineNumber() && b.endColumn() < a.startColumn())) {
            return false;
        }

        // These ranges must intersect
        return true;
    }

    /**
     * Test if two ranges are intersecting or touching. If the ranges are touching it is considered an intersection.
     */
    static bool areIntersectingOrTouching(const IRange& a, const IRange& b) {
        // Check if `a` is before `b`
        if (a.endLineNumber() < b.startLineNumber() || (a.endLineNumber() == b.startLineNumber() && a.endColumn() < b.startColumn())) {
            return false;
        }

        // Check if `b` is before `a`
        if (b.endLineNumber() < a.startLineNumber() || (b.endLineNumber() == a.startLineNumber() && b.endColumn() < a.startColumn())) {
            return false;
        }

        // These ranges must intersect
        return true;
    }

    /**
     * Compare two ranges and sort them by start position.
     */
    static int compareRangesUsingStarts(const IRange* a, const IRange* b) {
        if (!a && !b) {
            return 0;
        }
        if (!a) {
            return -1;
        }
        if (!b) {
            return 1;
        }

        if (a->startLineNumber() == b->startLineNumber()) {
            if (a->startColumn() == b->startColumn()) {
                if (a->endLineNumber() == b->endLineNumber()) {
                    return a->endColumn() - b->endColumn();
                }
                return a->endLineNumber() - b->endLineNumber();
            }
            return a->startColumn() - b->startColumn();
        }
        return a->startLineNumber() - b->startLineNumber();
    }

    /**
     * Test if the range spans multiple lines.
     */
    static bool spansMultipleLines(const IRange& range) {
        return range.endLineNumber() > range.startLineNumber();
    }

    // Operators for comparison
    bool operator==(const Range& other) const {
        return _startLineNumber == other._startLineNumber && _startColumn == other._startColumn &&
               _endLineNumber == other._endLineNumber && _endColumn == other._endColumn;
    }

    bool operator!=(const Range& other) const {
        return !(*this == other);
    }
};

} // namespace common
} // namespace textbuffer 