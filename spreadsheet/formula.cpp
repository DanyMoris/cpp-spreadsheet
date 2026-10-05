#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <sstream>

FormulaError::FormulaError(Category category)
    : category_(category) {
}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    switch (category_) {
        case Category::Ref:
            return "#REF!";
        case Category::Value:
            return "#VALUE!";
        case Category::Arithmetic:
            return "#ARITHM!";
    }
    return "";
}

std::ostream& operator<<(std::ostream& output, const FormulaError& fe) {
    return output << fe.ToString();
}

namespace {
double TextToNumber(const std::string& text) {
    if (text.empty()) {
        return 0.0;
    }
    double number = 0.0;
    std::istringstream in(text);
    in >> std::noskipws >> number;
    if (!in || !in.eof()) {
        throw FormulaError(FormulaError::Category::Value);
    }
    return number;
}

double CellToNumber(const CellInterface* cell) {
    if (!cell) {
        return 0.0;
    }
    const auto value = cell->GetValue();
    if (const auto* number = std::get_if<double>(&value)) {
        return *number;
    }
    if (const auto* error = std::get_if<FormulaError>(&value)) {
        throw *error;
    }
    return TextToNumber(std::get<std::string>(value));
}

std::vector<Position> CollectValidCells(const FormulaAST& ast) {
    std::vector<Position> cells;
    for (const Position pos : ast.GetCells()) {
        if (pos.IsValid()) {
            cells.push_back(pos);
        }
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    return cells;
}

class Formula final : public FormulaInterface {
public:
    explicit Formula(const std::string& expression)
        : ast_(ParseFormulaAST(expression))
        , cells_(CollectValidCells(ast_)) {
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        try {
            return ast_.Execute([&sheet](Position pos) {
                return CellToNumber(sheet.GetCell(pos));
            });
        } catch (const FormulaError& error) {
            return error;
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        return cells_;
    }

private:
    FormulaAST ast_;
    std::vector<Position> cells_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(expression);
}
