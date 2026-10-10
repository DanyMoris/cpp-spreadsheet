C++ Spreadsheet
A spreadsheet engine written in C++17. It provides a cell-based worksheet with formula evaluation, cell references, dependency tracking, and tab-separated input/output.

Features
Store plain text, numbers, and formulas in cells.
Evaluate arithmetic formulas with parentheses, unary operators, and +, -, *, and /.
Reference other cells using spreadsheet addresses such as A1 and BC27.
Recalculate dependent formulas when cell contents change, using cached values.
Detect circular dependencies and report invalid references, non-numeric values, and arithmetic errors.
Print worksheet values or original cell contents as tab-separated rows.
Escape text beginning with = by prefixing it with an apostrophe.
Implementation
The worksheet maintains a dependency graph between cells so it can detect cycles and invalidate cached formula results when referenced cells change. Formula syntax is defined in spreadsheet/Formula.g4 and parsed with ANTLR. The project includes a small built-in test runner in spreadsheet/main.cpp; the current executable runs these tests rather than providing an interactive spreadsheet UI.

Build
Requirements:

CMake 3.8 or newer
A C++17 compiler
Java runtime
ANTLR 4.12.0 complete JAR and the ANTLR4 C++ runtime source
The ANTLR dependencies are not included in this repository. The current CMake configuration expects the JAR at spreadsheet/antlr-4.12.0-complete.jar and the C++ runtime source tree at spreadsheet/antlr4_runtime/.

From the repository root, after placing those dependencies at the expected paths, configure and build with:

cmake -S spreadsheet -B build
cmake --build build
Run the built-in tests by running the spreadsheet executable produced in the build directory (for example, ./build/spreadsheet on Linux).

Project layout
spreadsheet/sheet.* — worksheet implementation and cell dependency graph
spreadsheet/cell.* — cell contents, formula values, and result caching
spreadsheet/formula.*, spreadsheet/FormulaAST.* — formula parsing and evaluation
spreadsheet/Formula.g4 — ANTLR grammar for formulas
spreadsheet/main.cpp — built-in tests
