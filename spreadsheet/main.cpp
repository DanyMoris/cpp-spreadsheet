#include "common.h"
#include "test_runner_p.h"

inline std::ostream& operator<<(std::ostream& output, Position pos) {
    return output << "(" << pos.row << ", " << pos.col << ")";
}

inline Position operator"" _pos(const char* str, std::size_t) {
    return Position::FromString(str);
}

inline std::ostream& operator<<(std::ostream& output, Size size) {
    return output << "(" << size.rows << ", " << size.cols << ")";
}

inline std::ostream& operator<<(std::ostream& output, const CellInterface::Value& value) {
    std::visit(
        [&](const auto& x) {
            output << x;
        },
        value);
    return output;
}

namespace {

void TestEmpty() {
    auto sheet = CreateSheet();
    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{0, 0}));
}

void TestInvalidPosition() {
    auto sheet = CreateSheet();
    try {
        sheet->SetCell(Position{-1, 0}, "");
    } catch (const InvalidPositionException&) {
    }
    try {
        sheet->GetCell(Position{0, -2});
    } catch (const InvalidPositionException&) {
    }
    try {
        sheet->ClearCell(Position{Position::MAX_ROWS, 0});
    } catch (const InvalidPositionException&) {
    }
}

void TestSetCellPlainText() {
    auto sheet = CreateSheet();

    auto checkCell = [&](Position pos, std::string text) {
        sheet->SetCell(pos, text);
        CellInterface* cell = sheet->GetCell(pos);
        ASSERT(cell != nullptr);
        ASSERT_EQUAL(cell->GetText(), text);
        ASSERT_EQUAL(std::get<std::string>(cell->GetValue()), text);
    };

    checkCell("A1"_pos, "Hello");
    checkCell("A1"_pos, "World");
    checkCell("B2"_pos, "Purr");
    checkCell("A3"_pos, "Meow");

    const SheetInterface& constSheet = *sheet;
    ASSERT_EQUAL(constSheet.GetCell("B2"_pos)->GetText(), "Purr");

    sheet->SetCell("A3"_pos, "'=escaped");
    CellInterface* cell = sheet->GetCell("A3"_pos);
    ASSERT_EQUAL(cell->GetText(), "'=escaped");
    ASSERT_EQUAL(std::get<std::string>(cell->GetValue()), "=escaped");
}

void TestClearCell() {
    auto sheet = CreateSheet();

    sheet->SetCell("C2"_pos, "Me gusta");
    sheet->ClearCell("C2"_pos);
    ASSERT(sheet->GetCell("C2"_pos) == nullptr);

    sheet->ClearCell("A1"_pos);
    sheet->ClearCell("J10"_pos);
}
void TestPrint() {
    auto sheet = CreateSheet();
    sheet->SetCell("A2"_pos, "meow");
    sheet->SetCell("B2"_pos, "=1+2");
    sheet->SetCell("A1"_pos, "=1/0");

    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{2, 2}));

    std::ostringstream texts;
    sheet->PrintTexts(texts);
    ASSERT_EQUAL(texts.str(), "=1/0\t\nmeow\t=1+2\n");

    std::ostringstream values;
    sheet->PrintValues(values);
    ASSERT_EQUAL(values.str(), "#ARITHM!\t\nmeow\t3\n");

    sheet->ClearCell("B2"_pos);
    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{2, 1}));
}

FormulaError::Category ErrorOf(const CellInterface::Value& value) {
    return std::get<FormulaError>(value).GetCategory();
}

void TestReferences() {
    auto sheet = CreateSheet();
    sheet->SetCell("A3"_pos, "=1+2*7");
    sheet->SetCell("A2"_pos, "3");
    sheet->SetCell("C2"_pos, "=A3/A2");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("A3"_pos)->GetValue()), 15);
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C2"_pos)->GetValue()), 5);
    ASSERT_EQUAL(sheet->GetCell("C2"_pos)->GetText(), "=A3/A2");
}

void TestEmptyCellIsZero() {
    auto sheet = CreateSheet();
    sheet->SetCell("B1"_pos, "=A5+1");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("B1"_pos)->GetValue()), 1);
    ASSERT(sheet->GetCell("A5"_pos) == nullptr);
    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{1, 2}));
}

void TestErrors() {
    auto sheet = CreateSheet();
    sheet->SetCell("A1"_pos, "=1");
    sheet->SetCell("A2"_pos, "text");
    sheet->SetCell("C2"_pos, "=A1/A2");
    sheet->SetCell("C4"_pos, "=C2+8");
    ASSERT(ErrorOf(sheet->GetCell("C2"_pos)->GetValue()) == FormulaError::Category::Value);
    ASSERT(ErrorOf(sheet->GetCell("C4"_pos)->GetValue()) == FormulaError::Category::Value);

    sheet->SetCell("D1"_pos, "=A1234567+ZZZZ1");
    ASSERT(ErrorOf(sheet->GetCell("D1"_pos)->GetValue()) == FormulaError::Category::Ref);
    ASSERT(sheet->GetCell("D1"_pos)->GetReferencedCells().empty());

    sheet->SetCell("D2"_pos, "=1/0");
    sheet->SetCell("D3"_pos, "=D2*2");
    ASSERT(ErrorOf(sheet->GetCell("D3"_pos)->GetValue()) == FormulaError::Category::Arithmetic);

    sheet->SetCell("A2"_pos, "2");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C4"_pos)->GetValue()), 8.5);
}

void TestReferencedCells() {
    auto sheet = CreateSheet();
    sheet->SetCell("C3"_pos, "=B2+A1+B2");
    const auto cells = sheet->GetCell("C3"_pos)->GetReferencedCells();
    ASSERT_EQUAL(cells.size(), 2u);
    ASSERT_EQUAL(cells[0], "A1"_pos);
    ASSERT_EQUAL(cells[1], "B2"_pos);
    sheet->SetCell("E1"_pos, "text");
    ASSERT(sheet->GetCell("E1"_pos)->GetReferencedCells().empty());
}

void TestBadFormulaKeepsCell() {
    auto sheet = CreateSheet();
    sheet->SetCell("A1"_pos, "ok");
    bool thrown = false;
    try {
        sheet->SetCell("A1"_pos, "=A2+*");
    } catch (const FormulaException&) {
        thrown = true;
    }
    ASSERT(thrown);
    ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetText(), "ok");

    thrown = false;
    try {
        sheet->SetCell("B7"_pos, "=1+");
    } catch (const FormulaException&) {
        thrown = true;
    }
    ASSERT(thrown);
    ASSERT(sheet->GetCell("B7"_pos) == nullptr);
    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{1, 1}));
}

void TestCycles() {
    auto sheet = CreateSheet();
    auto expect_cycle = [&](const char* pos, const char* text) {
        bool thrown = false;
        try {
            sheet->SetCell(Position::FromString(pos), text);
        } catch (const CircularDependencyException&) {
            thrown = true;
        }
        ASSERT(thrown);
    };

    expect_cycle("A1", "=A1");

    sheet->SetCell("A1"_pos, "=B1");
    sheet->SetCell("B1"_pos, "=C1");
    expect_cycle("C1", "=A1");
    expect_cycle("C1", "=1+B1*2");
    ASSERT(sheet->GetCell("C1"_pos) == nullptr);
    ASSERT_EQUAL(sheet->GetCell("B1"_pos)->GetText(), "=C1");

    sheet->SetCell("A3"_pos, "=1+2*7");
    sheet->SetCell("C4"_pos, "=C2");
    sheet->SetCell("C2"_pos, "=A3/A2");
    expect_cycle("A3", "=C4-1");
    ASSERT_EQUAL(sheet->GetCell("A3"_pos)->GetText(), "=1+2*7");

    sheet->SetCell("C1"_pos, "7");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("A1"_pos)->GetValue()), 7);
}

void TestCacheInvalidation() {
    auto sheet = CreateSheet();
    sheet->SetCell("A1"_pos, "1");
    sheet->SetCell("B1"_pos, "=A1+1");
    sheet->SetCell("C1"_pos, "=B1*2");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C1"_pos)->GetValue()), 4);

    sheet->SetCell("A1"_pos, "10");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("B1"_pos)->GetValue()), 11);
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C1"_pos)->GetValue()), 22);

    sheet->SetCell("B1"_pos, "=5");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C1"_pos)->GetValue()), 10);

    sheet->SetCell("B1"_pos, "=A1+1");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C1"_pos)->GetValue()), 22);
    sheet->ClearCell("A1"_pos);
    ASSERT(sheet->GetCell("A1"_pos) == nullptr);
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C1"_pos)->GetValue()), 2);
    sheet->SetCell("A1"_pos, "3");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell("C1"_pos)->GetValue()), 8);
}

void TestHiddenNodes() {
    auto sheet = CreateSheet();
    sheet->SetCell("B1"_pos, "=A1");
    ASSERT(sheet->GetCell("A1"_pos) == nullptr);
    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{1, 2}));
    sheet->SetCell("B1"_pos, "=C1");
    sheet->ClearCell("B1"_pos);
    ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{0, 0}));
    std::ostringstream out;
    sheet->PrintTexts(out);
    ASSERT_EQUAL(out.str(), "");
}

void TestLongChain() {
    auto sheet = CreateSheet();
    const int n = 5000;
    for (int row = n - 1; row >= 0; --row) {
        sheet->SetCell({row, 0}, "=" + Position{row + 1, 0}.ToString() + "+1");
    }
    ASSERT_EQUAL(std::get<double>(sheet->GetCell({0, 0})->GetValue()), n);
    sheet->SetCell({n, 0}, "5");
    ASSERT_EQUAL(std::get<double>(sheet->GetCell({0, 0})->GetValue()), n + 5);
}

}  // namespace

int main() {
    TestRunner tr;
    RUN_TEST(tr, TestEmpty);
    RUN_TEST(tr, TestInvalidPosition);
    RUN_TEST(tr, TestSetCellPlainText);
    RUN_TEST(tr, TestClearCell);
    RUN_TEST(tr, TestPrint);
    RUN_TEST(tr, TestReferences);
    RUN_TEST(tr, TestEmptyCellIsZero);
    RUN_TEST(tr, TestErrors);
    RUN_TEST(tr, TestReferencedCells);
    RUN_TEST(tr, TestBadFormulaKeepsCell);
    RUN_TEST(tr, TestCycles);
    RUN_TEST(tr, TestCacheInvalidation);
    RUN_TEST(tr, TestHiddenNodes);
    RUN_TEST(tr, TestLongChain);
}