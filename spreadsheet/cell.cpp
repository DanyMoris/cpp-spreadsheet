#include "cell.h"

#include "formula.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

class Cell::EmptyImpl final : public Impl {
public:
    Value GetValue(const SheetInterface&) const override {
        return std::string();
    }

    std::string GetText() const override {
        return std::string();
    }
};

class Cell::TextImpl final : public Impl {
public:
    explicit TextImpl(std::string text)
        : text_(std::move(text)) {
    }

    Value GetValue(const SheetInterface&) const override {
        if (text_.front() == ESCAPE_SIGN) {
            return text_.substr(1);
        }
        return text_;
    }

    std::string GetText() const override {
        return text_;
    }

private:
    std::string text_;
};

class Cell::FormulaImpl final : public Impl {
public:
    explicit FormulaImpl(std::unique_ptr<FormulaInterface> formula)
        : formula_(std::move(formula))
        , text_(FORMULA_SIGN + formula_->GetExpression()) {
    }

    Value GetValue(const SheetInterface& sheet) const override {
        if (!cache_) {
            cache_ = std::visit([](auto&& result) -> Value { return result; },
                                formula_->Evaluate(sheet));
        }
        return *cache_;
    }

    std::string GetText() const override {
        return text_;
    }

    std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }

    bool InvalidateCache() override {
        const bool was_valid = cache_.has_value();
        cache_.reset();
        return was_valid;
    }

    bool NeedsEvaluation() const override {
        return !cache_.has_value();
    }

private:
    std::unique_ptr<FormulaInterface> formula_;
    std::string text_;
    mutable std::optional<Value> cache_;
};

Cell::Cell(const SheetInterface& sheet)
    : sheet_(sheet)
    , impl_(std::make_unique<EmptyImpl>()) {
}

Cell::~Cell() = default;

Cell::Value Cell::GetValue() const {
    EvaluateDependencies();
    return impl_->GetValue(sheet_);
}

void Cell::EvaluateDependencies() const {
    if (!impl_->NeedsEvaluation()) {
        return;
    }
    std::vector<const Cell*> stack{this};
    while (!stack.empty()) {
        const Cell* cell = stack.back();
        if (!cell->impl_->NeedsEvaluation()) {
            stack.pop_back();
            continue;
        }
        bool ready = true;
        for (const Position pos : cell->impl_->GetReferencedCells()) {
            // Все ячейки таблицы - объекты Cell, а скрытые узлы GetCell не отдаёт.
            const auto* ref = static_cast<const Cell*>(sheet_.GetCell(pos));
            if (ref && ref->impl_->NeedsEvaluation()) {
                stack.push_back(ref);
                ready = false;
            }
        }
        if (ready) {
            cell->impl_->GetValue(sheet_);
            stack.pop_back();
        }
    }
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

std::unique_ptr<Cell::Impl> Cell::CreateImpl(std::string text) {
    if (text.empty()) {
        return std::make_unique<EmptyImpl>();
    }
    if (text.size() > 1 && text.front() == FORMULA_SIGN) {
        return std::make_unique<FormulaImpl>(ParseFormula(text.substr(1)));
    }
    return std::make_unique<TextImpl>(std::move(text));
}

void Cell::SetImpl(std::unique_ptr<Impl> impl) {
    impl_ = std::move(impl);
}

bool Cell::InvalidateCache() {
    return impl_->InvalidateCache();
}

bool Cell::IsEmpty() const {
    return impl_->GetText().empty();
}
