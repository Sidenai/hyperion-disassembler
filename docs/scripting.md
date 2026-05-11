# Scripting

Hyperion embeds Lua 5.4 for automation and custom analysis. Open the console via **View > Script Console** or write `.lua` files and paste them in.

## API

### Navigation

```lua
goto_addr(0x140001000)
```
Moves the disassembly view and syncs all panels to the given address.

### Names

```lua
local name = get_name(0x140001000)
set_name(0x140001000, "initialize_app")
```
Read or write the symbol name at any address. Works for functions, imports, globals.

### Comments

```lua
set_comment(0x140001000, "called on startup")
```

### Instructions

```lua
local insn = get_insn(0x140001000)
print(insn.mnemonic)   -- "mov"
print(insn.op_str)     -- "rax, rcx"
print(insn.addr)       -- address
print(insn.len)        -- byte length
```
Returns nil if no instruction at that address.

### Raw bytes

```lua
local hex = get_bytes(0x140001000, 16)
print(hex)  -- "48 89 5C 24 08 ..."
```
Returns a hex string of N bytes starting at addr.

### Functions

```lua
local funcs = get_functions()
for _, addr in ipairs(funcs) do
    print(string.format("%X: %s", addr, get_name(addr)))
end
```
Returns a table of all function entry addresses.

### Cross-references

```lua
local xrefs = get_xrefs_to(0x140001000)
for _, src in ipairs(xrefs) do
    print(string.format("referenced from %X", src))
end
```
Returns a table of addresses that reference the target.

### Output

```lua
print("hello")  -- prints to the script console output
```

## Examples

### Rename all `sub_` functions to include their RVA

```lua
local base = 0x140000000
for _, addr in ipairs(get_functions()) do
    local name = get_name(addr)
    if name:find("^sub_") then
        set_name(addr, string.format("fn_%X", addr - base))
    end
end
```

### Find who calls a specific function

```lua
local target = 0x140005000
local refs = get_xrefs_to(target)
print(string.format("%d callers:", #refs))
for _, r in ipairs(refs) do
    print(string.format("  %X  %s", r, get_name(r)))
end
```

### Annotate all indirect calls

```lua
for _, addr in ipairs(get_functions()) do
    local insn = get_insn(addr)
    if insn and insn.mnemonic == "call" and insn.op_str:find("%[") then
        set_comment(addr, "indirect call")
    end
end
```

### Dump function prologue bytes

```lua
local entry = 0x140001000
local bytes = get_bytes(entry, 8)
print(string.format("%s: %s", get_name(entry), bytes))
```

### Bulk search for error strings

```lua
for _, addr in ipairs(get_functions()) do
    local name = get_name(addr)
    if name:lower():find("error") or name:lower():find("fail") then
        print(string.format("%X  %s", addr, name))
    end
end
```

## Notes

- Scripts run on the main thread. Long loops will freeze the UI.
- All addresses are integers (use hex with `0x` prefix).
- Changes from scripts (renames, comments) are undoable with Ctrl+Z.
- The console keeps command history (up/down arrows).
