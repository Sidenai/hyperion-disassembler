# Hyperion

A native x86/x64 disassembler and binary analysis tool for Windows PE files. Built from scratch in C++20 with ImGui.

[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord)](https://discord.gg/yjym2b7A)
[![GitHub](https://img.shields.io/github/stars/mylovereturns/hyperion-disassembler?style=flat&label=Stars)](https://github.com/mylovereturns/hyperion-disassembler)

Hyperion performs recursive descent disassembly, automatic function detection, control flow graph construction, cross-reference analysis, RTTI class recovery, and decompilation — all parallelized across available cores.
<img width="2891" height="1146" alt="image" src="https://github.com/user-attachments/assets/a0cb18b1-109a-4e56-8aa8-58d34ced57af" />

## Community

- [Discord](https://discord.gg/yjym2b7A)
- [GitHub](https://github.com/mylovereturns/hyperion-disassembler)

## Features

**Analysis**
- Recursive descent + linear sweep with conflict resolution
- .pdata exception directory for accurate x64 function boundaries
- RTTI C++ class recovery (vtable parsing, method naming, class hierarchy)
- Import thunk detection, switch/jump table resolution
- Vtable detection, global variable identification
- FLIRT-style signature matching (~100 MSVC CRT patterns)
- Packer detection (UPX, Themida, VMProtect, ASPack, MPRESS)
- PDB symbol loading (auto-detect .pdb, applies names/types via DbgHelp)
- C++ name demangling
- Noreturn detection, tail call detection, calling convention inference
- Dataflow propagation for indirect call resolution
- Inter-procedural type propagation

**Decompiler**
- Ghidra-style architecture: p-code lift → SSA → DCE → propagation → structuring → emit
- Dead code elimination (stack frame setup, callee-saved regs, unused flags)
- Copy propagation and constant folding
- Control flow structuring (if/else, while, for, do-while)
- RTTI-aware: vtable calls shown as `obj->Class::method()`
- STL container recognition (std::string, std::vector, std::shared_ptr)
- Operator overload detection (infix notation for known operators)
- Return value propagation, caller-save argument folding
- Main/WinMain detection with typed parameters
- F5 to decompile current function

**UI**
- ImGui docking with 3 themes (Binary Ninja, IDA, Midnight)
- Disassembly view with color-coded mnemonics, inline string/import annotations, xref badges
- Hex editor with byte patching, pattern highlight, keyboard navigation
- Pseudo-code panel with copy support
- Control flow graph with clickable nodes and instruction-level navigation
- Functions, strings, imports/exports panels with filter and copy
- Cross-reference popup (X key) and tabbed panel
- Entropy heatmap, call graph view
- Stack frame view with var_XX/arg_XX naming
- Type system (structs, enums), classes view (RTTI browser)
- Binary diff (compare two PEs)
- PE header viewer with packer detection results
- Search: text, binary pattern (wildcards), immediate values
- Beautify mode (hides noise, shows only function code + key data)
- Context menu: copy as C array, Python bytes, YARA pattern
- Export: patched binary, .asm listing, IDAPython script, project save/load

**Scripting**
- Embedded Lua 5.4 console (View > Script Console)
- API: get_name, set_name, get_insn, get_bytes, get_functions, get_xrefs_to, set_comment, goto_addr
- See [docs/scripting.md](docs/scripting.md) for full reference

**Stability**
- PE loader hardened against malformed binaries
- Thread-safe analysis (atomic handoff to UI thread)
- Memory optimized (fixed-size instruction buffers)
- Full undo/redo for all user operations
- Auto-save every 60 seconds

## Keybinds

| Key | Action |
|-----|--------|
| G | Go to address |
| N | Rename |
| ; | Comment |
| X | Cross-references |
| F5 | Decompile function |
| D | Define data (cycle byte/word/dword/qword) |
| A | Define ASCII string |
| U | Undefine |
| C | Force as code |
| H | Toggle hex/decimal |
| Enter | Follow branch/call |
| Escape | Navigate back |
| Space/Tab | Sync graph + pseudo-code |
| Ctrl+O | Open |
| Ctrl+S | Save project |
| Ctrl+F | Search |
| Alt+B | Binary pattern search |
| Ctrl+Z/Y | Undo/Redo |
| Ctrl+C | Copy (pseudo-code panel) |

## Building

Requires CMake 3.25+, vcpkg, MSVC 2022+.

```
git clone https://github.com/mylovereturns/hyperion-disassembler
cd hyperion-disassembler
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Dependencies pulled via vcpkg: imgui (docking), glfw, zydis, spdlog, fmt, zlib, lua.

## Status

Active development. Functional for static analysis of Windows PE binaries. The decompiler produces readable C-like output for most functions under 200 basic blocks. RTTI class recovery works on unobfuscated C++ binaries.

Roadmap:
- ELF support
- Debugger integration
- Plugin API
- Full FLIRT .sig tree parsing

## License

MIT
