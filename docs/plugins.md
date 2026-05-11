# Plugins

Hyperion loads Lua plugins from the `plugins/` directory next to the executable on startup.

## Creating a Plugin

Create a `.lua` file in `plugins/`:

```lua
-- plugins/my_plugin.lua
register_plugin("My Plugin", "Description of what it does")

register_menu_item("Do Something", function()
    print("Running my plugin action")
    -- your code here
end)
```

## Registration Functions

### register_plugin(name, description)
Declares your plugin. Call this first.

### register_menu_item(label, callback)
Adds an entry to the Plugins menu. Clicking it runs your callback.

```lua
register_menu_item("Rename all sub_ to fn_", function()
    local base = get_image_base()
    for _, addr in ipairs(get_functions()) do
        local name = get_name(addr)
        if name:find("^sub_") then
            set_name(addr, "fn_" .. string.format("%X", addr - base))
        end
    end
    print("done")
end)
```

### register_hotkey(key_string, callback)
Binds a keyboard shortcut.

```lua
register_hotkey("Ctrl+Shift+R", function()
    print("hotkey triggered at " .. string.format("%X", get_cursor()))
end)
```

### register_on_analysis_complete(callback)
Runs your callback after analysis finishes on a new binary.

```lua
register_on_analysis_complete(function()
    local funcs = get_functions()
    print(string.format("Analysis done: %d functions", #funcs))
end)
```

## Full API Reference

### Navigation
```lua
goto_addr(0x140001000)    -- navigate disasm + sync all panels
```

### Names & Comments
```lua
get_name(addr)            -- returns symbol name (or "")
set_name(addr, "name")    -- rename address
set_comment(addr, "text") -- add comment
get_comment(addr)         -- read comment
```

### Instructions
```lua
local i = get_insn(addr)
i.addr                    -- address
i.mnemonic                -- "mov", "call", etc.
i.op_str                  -- operand string
i.len                     -- byte length
```

### Raw Data
```lua
get_bytes(addr, len)      -- hex string "48 89 5C 24 08"
patch_byte(addr, 0x90)    -- write a byte
```

### Functions
```lua
get_functions()           -- table of all function entry addresses
create_function(addr)     -- force create function at address
```

### Cross-references
```lua
get_xrefs_to(addr)        -- table of addresses that reference target
```

### Binary Info
```lua
get_arch()                -- "x64", "x86", "arm64", etc.
get_image_base()          -- base address
get_segments()            -- list of {name, addr, size, flags}
get_string(addr)          -- string at address (if detected)
```

### Output
```lua
print("message")          -- prints to script console
```

## Example Plugins

### Auto-annotate crypto constants
```lua
register_plugin("Crypto Finder", "Finds common crypto constants")

local crypto_consts = {
    [0x67452301] = "MD5_INIT_A",
    [0xEFCDAB89] = "MD5_INIT_B",
    [0x98BADCFE] = "MD5_INIT_C",
    [0x10325476] = "MD5_INIT_D",
    [0x6A09E667] = "SHA256_H0",
}

register_menu_item("Find Crypto Constants", function()
    for _, addr in ipairs(get_functions()) do
        local insn = get_insn(addr)
        if insn then
            for const, name in pairs(crypto_consts) do
                if insn.op_str:find(string.format("%X", const)) then
                    set_comment(addr, name)
                    print(string.format("Found %s at %X", name, addr))
                end
            end
        end
    end
end)
```

### Export function list to file
```lua
register_plugin("Exporter", "Export data to files")

register_menu_item("Export Functions to CSV", function()
    local f = io.open("functions.csv", "w")
    if not f then print("cannot open file"); return end
    f:write("address,name\n")
    for _, addr in ipairs(get_functions()) do
        f:write(string.format("0x%X,%s\n", addr, get_name(addr)))
    end
    f:close()
    print("exported to functions.csv")
end)
```

### Highlight suspicious patterns
```lua
register_plugin("Suspicious", "Find suspicious code patterns")

register_on_analysis_complete(function()
    for _, addr in ipairs(get_functions()) do
        local insn = get_insn(addr)
        if insn and insn.mnemonic == "int" and insn.op_str == "0x2E" then
            set_comment(addr, "SUSPICIOUS: int 2Eh syscall")
            print(string.format("Found int 2Eh at %X", addr))
        end
    end
end)
```

## Notes

- Plugins load alphabetically on startup
- Errors in plugins are shown in the Script Console output
- Plugin callbacks run on the main thread — keep them fast
- Changes made by plugins are undoable (Ctrl+Z)
- Plugins can call any API function available in the Script Console
