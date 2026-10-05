#pragma once

#include "common.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class Sheet;

class Cell : public CellInterface {
public:
    explicit Cell(const SheetInterface& sheet);
    ~Cell();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

private:
    friend class Sheet;

    class Impl {
    public:
        virtual ~Impl() = default;
        virtual Value GetValue(const SheetInterface& sheet) const = 0;
        virtual std::string GetText() const = 0;
        virtual std::vector<Position> GetReferencedCells() const {
            return {};
        }
        virtual bool InvalidateCache() {
            return false;
        }
        virtual bool NeedsEvaluation() const {
            return false;
        }
    };

    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    // Разбирает текст в содержимое ячейки. Может бросить FormulaException.
    // Состояния ячеек и таблицы не затрагивает.
    static std::unique_ptr<Impl> CreateImpl(std::string text);

    // Заменяет содержимое ячейки; кэш нового содержимого пуст.
    void SetImpl(std::unique_ptr<Impl> impl);

    // Сбрасывает кэш. Возвращает true, если в кэше было значение.
    bool InvalidateCache();

    bool IsEmpty() const;

    // Заполняет кэши всех ячеек, от которых транзитивно зависит эта, в порядке
    // зависимостей (обход итеративный, без рекурсии по цепочке ячеек).
    void EvaluateDependencies() const;

    const SheetInterface& sheet_;
    std::unique_ptr<Impl> impl_;

    // Рёбра графа зависимостей:
    //  - исходящие (ячейки, которые читает формула) берутся из
    //    GetReferencedCells(), они хранятся в самой формуле;
    //  - входящие: ячейки, формулы которых читают эту ячейку.
    std::unordered_set<Cell*> dependents_;

    // Ячейка создана только как узел графа для ссылки из формулы (или
    // очищена, но на неё ещё кто-то ссылается); наружу не видна.
    bool hidden_ = false;
};
