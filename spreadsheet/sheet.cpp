#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_set>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::ValidatePosition(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }
}

Cell* Sheet::FindNode(Position pos) const {
    const auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

bool Sheet::WouldCreateCycle(Position pos, const std::vector<Position>& refs) const {
    if (std::find(refs.begin(), refs.end(), pos) != refs.end()) {
        return true;
    }
    const Cell* target = FindNode(pos);
    if (!target) {
        return false;
    }

    std::unordered_set<const Cell*> visited;
    std::vector<const Cell*> stack;
    const auto visit = [&](Position ref) {
        const Cell* node = FindNode(ref);
        if (node && visited.insert(node).second) {
            stack.push_back(node);
        }
    };

    for (const Position ref : refs) {
        visit(ref);
    }
    while (!stack.empty()) {
        const Cell* node = stack.back();
        stack.pop_back();
        if (node == target) {
            return true;
        }
        for (const Position ref : node->GetReferencedCells()) {
            visit(ref);
        }
    }
    return false;
}

void Sheet::InvalidateDependents(Cell* cell) {
    std::vector<Cell*> stack(cell->dependents_.begin(), cell->dependents_.end());
    while (!stack.empty()) {
        Cell* node = stack.back();
        stack.pop_back();
        if (node->InvalidateCache()) {
            stack.insert(stack.end(), node->dependents_.begin(), node->dependents_.end());
        }
    }
}

void Sheet::Unlink(Cell* cell, const std::vector<Position>& old_refs) {
    for (const Position ref : old_refs) {
        if (Cell* node = FindNode(ref)) {
            node->dependents_.erase(cell);
        }
    }
}

void Sheet::EraseIfUnused(Position pos) {
    const Cell* node = FindNode(pos);
    if (node && node->hidden_ && node->dependents_.empty()) {
        cells_.erase(pos);
    }
}

void Sheet::UpdateCounts(Position pos, bool was_non_empty, bool is_non_empty) {
    if (was_non_empty == is_non_empty) {
        return;
    }
    const int delta = is_non_empty ? 1 : -1;
    for (auto [counts, key] : {std::pair{&row_counts_, pos.row}, std::pair{&col_counts_, pos.col}}) {
        const int count = ((*counts)[key] += delta);
        if (count == 0) {
            counts->erase(key);
        }
    }
}

void Sheet::SetCell(Position pos, std::string text) {
    ValidatePosition(pos);

    Cell* cell = FindNode(pos);
    if (cell && !cell->hidden_ && cell->GetText() == text) {
        return;
    }

    auto impl = Cell::CreateImpl(std::move(text));
    const std::vector<Position> refs = impl->GetReferencedCells();
    if (WouldCreateCycle(pos, refs)) {
        throw CircularDependencyException("Circular dependency at " + pos.ToString());
    }

    if (!cell) {
        cell = cells_.emplace(pos, std::make_unique<Cell>(*this)).first->second.get();
        cell->hidden_ = true;
    }
    for (const Position ref : refs) {
        if (!FindNode(ref)) {
            cells_.emplace(ref, std::make_unique<Cell>(*this)).first->second->hidden_ = true;
        }
    }

    const bool was_non_empty = !cell->IsEmpty();
    const std::vector<Position> old_refs = cell->GetReferencedCells();
    Unlink(cell, old_refs);

    cell->SetImpl(std::move(impl));
    cell->hidden_ = false;
    for (const Position ref : refs) {
        FindNode(ref)->dependents_.insert(cell);
    }

    UpdateCounts(pos, was_non_empty, !cell->IsEmpty());
    InvalidateDependents(cell);
    for (const Position old_ref : old_refs) {
        EraseIfUnused(old_ref);
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    ValidatePosition(pos);
    const Cell* cell = FindNode(pos);
    return cell && !cell->hidden_ ? cell : nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    ValidatePosition(pos);
    Cell* cell = FindNode(pos);
    return cell && !cell->hidden_ ? cell : nullptr;
}

void Sheet::ClearCell(Position pos) {
    ValidatePosition(pos);
    Cell* cell = FindNode(pos);
    if (!cell || cell->hidden_) {
        return;
    }

    const bool was_non_empty = !cell->IsEmpty();
    const std::vector<Position> old_refs = cell->GetReferencedCells();
    Unlink(cell, old_refs);

    cell->SetImpl(Cell::CreateImpl(""));
    UpdateCounts(pos, was_non_empty, false);
    InvalidateDependents(cell);

    if (cell->dependents_.empty()) {
        cells_.erase(pos);
    } else {
        cell->hidden_ = true;
    }
    for (const Position old_ref : old_refs) {
        EraseIfUnused(old_ref);
    }
}

Size Sheet::GetPrintableSize() const {
    if (row_counts_.empty()) {
        return {0, 0};
    }
    return {row_counts_.rbegin()->first + 1, col_counts_.rbegin()->first + 1};
}

void Sheet::Print(std::ostream& output, const std::function<void(const Cell&)>& print_cell) const {
    const Size size = GetPrintableSize();
    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            if (const Cell* cell = FindNode({row, col})) {
                print_cell(*cell);
            }
        }
        output << '\n';
    }
}

void Sheet::PrintValues(std::ostream& output) const {
    Print(output, [&output](const Cell& cell) {
        std::visit([&output](const auto& value) { output << value; }, cell.GetValue());
    });
}

void Sheet::PrintTexts(std::ostream& output) const {
    Print(output, [&output](const Cell& cell) { output << cell.GetText(); });
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}
