#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

class Sheet : public SheetInterface {
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

private:
    struct PositionHash {
        size_t operator()(Position pos) const {
            return static_cast<size_t>(pos.row) * Position::MAX_COLS + pos.col;
        }
    };

    static void ValidatePosition(Position pos);

    // Узел графа по позиции или nullptr. Возвращает в том числе скрытые узлы.
    Cell* FindNode(Position pos) const;

    // Содержит ли (непустая) формула, записываемая в pos, путь обратно в pos.
    // Обход по исходящим рёбрам, каждая ячейка посещается не более одного раза.
    bool WouldCreateCycle(Position pos, const std::vector<Position>& refs) const;

    // Сбрасывает кэши всех ячеек, транзитивно зависящих от cell. Обход по
    // входящим рёбрам; ячейка с уже пустым кэшем отсекает свою ветку.
    static void InvalidateDependents(Cell* cell);

    void Unlink(Cell* cell, const std::vector<Position>& old_refs);
    void EraseIfUnused(Position pos);
    void UpdateCounts(Position pos, bool was_non_empty, bool is_non_empty);

    void Print(std::ostream& output, const std::function<void(const Cell&)>& print_cell) const;

    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHash> cells_;

    // Количество непустых ячеек в каждой строке/столбце: размер печатной
    // области - это последние ключи.
    std::map<int, int> row_counts_;
    std::map<int, int> col_counts_;
};
