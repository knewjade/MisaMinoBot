#include "moves.hpp"

namespace core {
    namespace {
        uint64_t getXMask(int x, int y) {
            assert(0 <= x && x < FIELD_WIDTH);
            assert(0 <= y && y < MAX_FIELD_HEIGHT);

            return 1LLU << (x + y * FIELD_WIDTH);
        }
    }

    void Cache::visit(int x, int y, RotateType rotateType) {
        assert(0 <= x && x < FIELD_WIDTH);
        assert(0 <= y && y < MAX_FIELD_HEIGHT);

        int index = y / 6;
        uint64_t mask = getXMask(x, y - 6 * index);

        int boardIndex = index + 4 * rotateType;
        visitedBoard[boardIndex] |= mask;
    }

    bool Cache::isVisit(int x, int y, core::RotateType rotateType) const {
        assert(0 <= x && x < FIELD_WIDTH);
        assert(0 <= y && y < MAX_FIELD_HEIGHT);

        int index = y / 6;
        uint64_t mask = getXMask(x, y - 6 * index);

        int boardIndex = index + 4 * rotateType;
        return (visitedBoard[boardIndex] & mask) != 0;
    }

    void Cache::found(int x, int y, RotateType rotateType) {
        assert(0 <= x && x < FIELD_WIDTH);
        assert(0 <= y && y < MAX_FIELD_HEIGHT);

        int index = y / 6;
        uint64_t mask = getXMask(x, y - 6 * index);

        int boardIndex = index + 4 * rotateType;
        foundBoard[boardIndex] |= mask;
    }

    bool Cache::isFound(int x, int y, core::RotateType rotateType) const {
        assert(0 <= x && x < FIELD_WIDTH);
        assert(0 <= y && y < MAX_FIELD_HEIGHT);

        int index = y / 6;
        uint64_t mask = getXMask(x, y - 6 * index);

        int boardIndex = index + 4 * rotateType;
        return (foundBoard[boardIndex] & mask) != 0;
    }

    void Cache::clear() {
        for (int index = 0; index < 16; ++index) {
            visitedBoard[index] = 0;
        }
        for (int index = 0; index < 16; ++index) {
            foundBoard[index] = 0;
        }
    }

    void Cache::resetTrail() {
        for (int index = 0; index < 16; ++index) {
            visitedBoard[index] = 0;
        }
    }

    namespace harddrop {
        void MoveGenerator::search(
                std::vector<Move> &moves, const Field &field, const PieceType pieceType, int validHeight
        ) {
            auto &piece = factory.get(pieceType);

            auto bit = piece.uniqueRotateBit;
            assert(bit != 0);

            do {
                auto next = bit & (bit - 1);
                RotateType rotateType = rotateBitToVal[bit & ~next];

                auto &blocks = factory.get(pieceType, rotateType);

                int y = validHeight - blocks.minY;
                int maxY = validHeight - blocks.maxY;
                for (int x = -blocks.minX, maxX = FIELD_WIDTH - blocks.maxX; x < maxX; ++x) {
                    int harddropY = field.getYOnHarddrop(blocks, x, y);
                    if (harddropY < maxY) {
                        moves.push_back(Move{rotateType, x, harddropY, true});
                    }
                }

                bit = next;
            } while (bit != 0);
        }
    }

    namespace srs {
        void MoveGenerator::search(
                std::vector<Move> &moves, const Field &field, const PieceType pieceType, int validHeight
        ) {
            appearY = validHeight;

            cache.clear();

            auto &piece = factory.get(pieceType);
            auto target = TargetObject{field, piece};

            for (int rotate = 0; rotate < 4; ++rotate) {
                RotateType rotateType = static_cast<RotateType >(rotate);

                auto &blocks = factory.get(pieceType, rotateType);
                for (int x = -blocks.minX, maxX = FIELD_WIDTH - blocks.maxX; x < maxX; ++x) {
                    for (int y = validHeight - blocks.maxY - 1; -blocks.minY <= y; --y) {
                        if (field.canPut(blocks, x, y) && field.isOnGround(blocks, x, y)) {
                            auto result = check(target, blocks, x, y, From::None, true);
                            if (result != MoveResults::No) {
                                auto &transform = piece.transforms[rotateType];
                                RotateType newRotate = transform.toRotate;
                                int newX = x + transform.offset.x;
                                int newY = y + transform.offset.y;
                                if (!cache.isFound(newX, newY, newRotate)) {
                                    cache.found(newX, newY, newRotate);
                                    moves.push_back(Move{newRotate, newX, newY, result == MoveResults::Harddrop});
                                }
                            }
                            cache.resetTrail();
                        }
                    }
                }
            }
        }

        MoveResults MoveGenerator::checkLeftRotation(
                const TargetObject &targetObject, const Blocks &toBlocks, int toX, int toY
        ) {
            auto piece = targetObject.piece;
            auto field = targetObject.field;

            // Direction before left rotation
            RotateType fromRotate = static_cast<RotateType>((toBlocks.rotateType + 1) % 4);
            auto &fromBlocks = piece.blocks[fromRotate];

            // Change the direction to `from`
            int toLeftX = toX + fromBlocks.minX;
            int toLowerY = toY + fromBlocks.minY;

            auto head = fromRotate * 5;
            int width = FIELD_WIDTH - fromBlocks.width;
            for (int index = head; index < head + piece.offsetsSize; ++index) {
                auto &offset = piece.leftOffsets[index];
                int fromLeftX = toLeftX - offset.x;
                int fromLowerY = toLowerY - offset.y;
                if (0 <= fromLeftX && fromLeftX <= width && 0 <= fromLowerY &&
                    field.canPutAtMaskIndex(fromBlocks, fromLeftX, fromLowerY)) {
                    int fromX = toX - offset.x;
                    int fromY = toY - offset.y;
                    int srsResult = srs::left(field, piece, fromRotate, toBlocks, fromX, fromY);
                    if (srsResult == -1) {
                        continue;
                    }

                    auto &kicks = piece.leftOffsets[srsResult];
                    if (offset.x == kicks.x && offset.y == kicks.y) {
                        auto result = check(targetObject, fromBlocks, fromX, fromY, From::None, false);
                        if (result != MoveResults::No) {
                            return result;
                        }
                    }
                }
            }

            return MoveResults::No;
        }

        MoveResults MoveGenerator::checkRightRotation(
                const TargetObject &targetObject, const Blocks &toBlocks, int toX, int toY
        ) {
            auto piece = targetObject.piece;
            auto field = targetObject.field;

            // Direction before right rotation
            RotateType fromRotate = static_cast<RotateType>((toBlocks.rotateType + 3) % 4);
            auto &fromBlocks = piece.blocks[fromRotate];

            // Change the direction to `from`
            int toLeftX = toX + fromBlocks.minX;
            int toLowerY = toY + fromBlocks.minY;

            auto head = fromRotate * 5;
            int width = FIELD_WIDTH - fromBlocks.width;
            for (int index = head; index < head + piece.offsetsSize; ++index) {
                auto &offset = piece.rightOffsets[index];
                int fromLeftX = toLeftX - offset.x;
                int fromLowerY = toLowerY - offset.y;
                if (0 <= fromLeftX && fromLeftX <= width && 0 <= fromLowerY &&
                    field.canPutAtMaskIndex(fromBlocks, fromLeftX, fromLowerY)) {
                    int fromX = toX - offset.x;
                    int fromY = toY - offset.y;
                    int srsResult = srs::right(field, piece, fromRotate, toBlocks, fromX, fromY);
                    if (srsResult == -1) {
                        continue;
                    }

                    auto &kicks = piece.rightOffsets[srsResult];
                    if (offset.x == kicks.x && offset.y == kicks.y) {
                        auto result = check(targetObject, fromBlocks, fromX, fromY, From::None, false);
                        if (result != MoveResults::No) {
                            return result;
                        }
                    }
                }
            }

            return MoveResults::No;
        }

        MoveResults MoveGenerator::check(
                const TargetObject &targetObject, const Blocks &blocks, int x, int y, From from, bool isFirstCall
        ) {
            auto field = targetObject.field;

            // When reach by harddrop
            if (field.canReachOnHarddrop(blocks, x, y)) {
                return isFirstCall ? MoveResults::Harddrop : MoveResults::Softdrop;
            }

            // When reach the top
            if (appearY <= y) {
                return MoveResults::Softdrop;
            }

            RotateType rotate = blocks.rotateType;

            // Visited already
            if (cache.isVisit(x, y, rotate)) {
                // Return no when it has been visited and not found because it can be covered by other path
                return cache.isFound(x, y, rotate) ? MoveResults::Softdrop : MoveResults::No;
            }

            cache.visit(x, y, rotate);

            // Move up
            int upY = y + 1;
            if (upY < appearY && field.canPut(blocks, x, upY)) {
                auto result = check(targetObject, blocks, x, upY, From::None, false);
                if (result != MoveResults::No) {
                    return result;
                }
            }

            // Move left
            int leftX = x - 1;
            if (from != From::Left && -blocks.minX <= leftX && field.canPut(blocks, leftX, y)) {
                auto result = check(targetObject, blocks, leftX, y, From::Right, false);
                if (result != MoveResults::No) {
                    return result;
                }
            }

            // Move right
            int rightX = x + 1;
            if (from != From::Right && rightX < FIELD_WIDTH - blocks.maxX && field.canPut(blocks, rightX, y)) {
                auto result = check(targetObject, blocks, rightX, y, From::Left, false);
                if (result != MoveResults::No) {
                    return result;
                }
            }

            // Move the place where there is a possibility of rotating right
            {
                auto result = checkRightRotation(targetObject, blocks, x, y);
                if (result != MoveResults::No) {
                    return result;
                }
            }

            // Move the place where there is a possibility of rotating left
            {

                auto result = checkLeftRotation(targetObject, blocks, x, y);
                if (result != MoveResults::No) {
                    return result;
                }
            }

            return MoveResults::No;
        }
    }
}