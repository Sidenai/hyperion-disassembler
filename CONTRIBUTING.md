# Contributing to Hyperion

Thank you for contributing to Hyperion. We appreciate all help whether it is fixing bugs, adding architectures, improving the decompiler or cleaning up the UI.

## Getting Started

### 1. Development Setup
To build the project locally you need **CMake 3.25+**, **vcpkg** and a **C++20 compatible compiler** (MSVC 2022+ is recommended for Windows).

```bash
# Fork the repository on GitHub then clone your fork
git clone --recursive https://github.com/Sidenai/hyperion-disassembler.git
cd hyperion-disassembler

# Configure and build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 2. Issues & Bug Reports
Before starting on a major feature or fix check the [Issue Tracker](https://github.com/Sidenai/hyperion-disassembler/issues) to make sure nobody else is already working on it. 
*   **Bugs:** Provide steps to reproduce, the expected behavior and what actually happened. Include OS and compiler versions. If the bug relates to a specific binary providing a sample helps immensely.
*   **Features:** Open a discussion issue before spending time on a massive PR so we can make sure the feature aligns with the project roadmap.

## AI Code Generation

If you use an AI coding assistant like Cursor, Copilot or ChatGPT to help write your PR you are strictly responsible for making sure it follows these guidelines.

*   **Prompt context:** If you use an autonomous agent you must instruct it to read this file first.
*   **No comment spam:** AI tools love to over-comment code line by line. Strip out all redundant comments before submitting. We do not want `// initialize the variable` or `// loop through the array` in this codebase. If a PR is full of AI-generated comment spam it will be rejected immediately.
*   **Verify the code:** AIs often write outdated, unsafe or non-idiomatic C++. Make sure the generated code uses the modern C++20 features required below, handles bounds checking correctly and doesn't introduce memory leaks.

## Coding Rules

Hyperion is a high performance security tool. It adheres to a strict and minimalist C++ style. Follow these rules when writing code:

*   **Modern C++20:** Write clean standard C++20. Use `<ranges>` and standard `<algorithm>` where it makes sense. Avoid over-engineering, deep inheritance hierarchies and heavy abstractions.
*   **Compiler warnings:** Code must compile cleanly. Treat warnings as errors. We enforce strict warning levels (`/W4` on MSVC, `-Wall -Wextra` on GCC/Clang).
*   **No exceptions:** Binaries are inherently untrusted and malformed data is common. Do not throw exceptions. Use `std::expected` or `std::optional` for error handling.
*   **Memory & RAII:** Manage resources using RAII. Never use raw `new` and `delete`. Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) or value semantics.
*   **Fixed-width types:** When parsing binaries always use fixed-width integer types like `uint8_t`, `uint32_t` and `size_t`. Never use raw `int` or `long` for binary structures.
*   **Bounds checking:** Never trust offsets or sizes read from a binary file. Always validate bounds against the file or section size before reading or writing to memory to prevent crashes and exploits.
*   **Auto keyword:** Only use `auto` when the type is obvious from the right side of the assignment (like `std::make_unique`) or for complex iterators. Don't use it if it hides the underlying type.
*   **Concurrency:** Use the existing task scheduler and worker pool for parallel tasks. Do not spawn raw `std::thread` instances unless you are writing a long-running background service like the debug engine.
*   **Performance:** Keep hot paths optimized. Avoid unnecessary heap allocations inside loops (especially in the linear sweep and recursive descent stages). Pass by `const &` for non-trivial types.
*   **Naming conventions:** Use standard library style naming. This means `snake_case` for variables, functions and standard structs/classes.
*   **Comments:** Write comments that explain why the code does something rather than what it is doing.
*   **ImGui:** When adding UI components make sure to manage ImGui IDs properly using `##` suffixes to avoid focus conflicts.
*   **Magic numbers:** If you need a magic number it must match an external spec (PE, ELF, Mach-O) or just be a random 32/64-bit value for custom formats. Don't use joke constants like DEADBEEF, CAFEBABE or FEEDFACE since it causes ambiguity and makes the format too easy to fingerprint.

## Pull Request Process

1.  Create a feature branch from `main` (`git checkout -b feature/my-cool-feature`).
2.  Make your changes and keep commits logically grouped.
3.  Ensure the project still builds across Windows, Linux and macOS. If you modify the decompiler or analysis engine test it against a few different binaries to catch regressions.
4.  Push your branch to your fork and open a Pull Request.
5.  Ensure all automated GitHub Actions CI checks pass.

## Community

If you need help or want to discuss architecture join our [Discord server](https://discord.gg/yjym2b7A).
