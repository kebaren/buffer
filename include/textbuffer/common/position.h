#pragma once

#include <cstdint>
#include <string>

namespace textbuffer {
namespace common {

/**
 * A position in the editor. This interface is suitable for serialization.
 */
class IPosition {
public:
    virtual ~IPosition() = default;

    /**
     * line number (starts at 1)
     */
    virtual int32_t lineNumber() const = 0;

    /**
     * column (the first character in a line is between column 1 and column 2)
     */
    virtual int32_t column() const = 0;
};

/**
 * A position in the editor.
 */
class Position : public IPosition {
private:
    int32_t _lineNumber;
    int32_t _column;

public:
    /**
     * Create a new position object.
     * 
     * @param lineNumber line number (starts at 1)
     * @param column column (starts at 1)
     */
    Position(int32_t lineNumber, int32_t column) 
        : _lineNumber(lineNumber), _column(column) {}

    /**
     * line number (starts at 1)
     */
    int32_t lineNumber() const override {
        return _lineNumber;
    }

    /**
     * column (the first character in a line is between column 1 and column 2)
     */
    int32_t column() const override {
        return _column;
    }

    /**
     * Create a new position from this position.
     *
     * @param newLineNumber new line number
     * @param newColumn new column
     */
    Position with(int32_t newLineNumber = -1, int32_t newColumn = -1) const {
        if (newLineNumber == -1) {
            newLineNumber = _lineNumber;
        }
        if (newColumn == -1) {
            newColumn = _column;
        }

        if (newLineNumber == _lineNumber && newColumn == _column) {
            return *this;
        } else {
            return Position(newLineNumber, newColumn);
        }
    }

    /**
     * Derive a new position from this position.
     *
     * @param deltaLineNumber line number delta
     * @param deltaColumn column delta
     */
    Position delta(int32_t deltaLineNumber = 0, int32_t deltaColumn = 0) const {
        return with(_lineNumber + deltaLineNumber, _column + deltaColumn);
    }

    /**
     * Test if this position equals other position
     */
    bool equals(const IPosition& other) const {
        return Position::equals(*this, other);
    }

    /**
     * Test if position `a` equals position `b`
     */
    static bool equals(const IPosition& a, const IPosition& b) {
        return a.lineNumber() == b.lineNumber() && a.column() == b.column();
    }

    /**
     * Test if this position is before other position.
     * If the two positions are equal, the result will be false.
     */
    bool isBefore(const IPosition& other) const {
        return Position::isBefore(*this, other);
    }

    /**
     * Test if position `a` is before position `b`.
     * If the two positions are equal, the result will be false.
     */
    static bool isBefore(const IPosition& a, const IPosition& b) {
        if (a.lineNumber() < b.lineNumber()) {
            return true;
        }
        if (b.lineNumber() < a.lineNumber()) {
            return false;
        }
        return a.column() < b.column();
    }

    /**
     * Test if this position is before other position.
     * If the two positions are equal, the result will be true.
     */
    bool isBeforeOrEqual(const IPosition& other) const {
        return Position::isBeforeOrEqual(*this, other);
    }

    /**
     * Test if position `a` is before position `b`.
     * If the two positions are equal, the result will be true.
     */
    static bool isBeforeOrEqual(const IPosition& a, const IPosition& b) {
        if (a.lineNumber() < b.lineNumber()) {
            return true;
        }
        if (b.lineNumber() < a.lineNumber()) {
            return false;
        }
        return a.column() <= b.column();
    }

    /**
     * A function that compares positions, useful for sorting
     */
    static int compare(const IPosition& a, const IPosition& b) {
        int32_t aLineNumber = a.lineNumber();
        int32_t bLineNumber = b.lineNumber();

        if (aLineNumber == bLineNumber) {
            int32_t aColumn = a.column();
            int32_t bColumn = b.column();
            return aColumn - bColumn;
        }

        return aLineNumber - bLineNumber;
    }

    /**
     * Clone this position.
     */
    Position clone() const {
        return Position(_lineNumber, _column);
    }

    /**
     * Convert to a human-readable representation.
     */
    std::string toString() const {
        return "(" + std::to_string(_lineNumber) + "," + std::to_string(_column) + ")";
    }

    /**
     * Create a `Position` from an `IPosition`.
     */
    static Position lift(const IPosition& pos) {
        return Position(pos.lineNumber(), pos.column());
    }

    // Operators for comparison
    bool operator==(const Position& other) const {
        return _lineNumber == other._lineNumber && _column == other._column;
    }

    bool operator!=(const Position& other) const {
        return !(*this == other);
    }

    bool operator<(const Position& other) const {
        return isBefore(other);
    }

    bool operator<=(const Position& other) const {
        return isBeforeOrEqual(other);
    }

    bool operator>(const Position& other) const {
        return other.isBefore(*this);
    }

    bool operator>=(const Position& other) const {
        return other.isBeforeOrEqual(*this);
    }
};

} // namespace common
} // namespace textbuffer 