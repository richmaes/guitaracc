---
name: guitaracc-cli-commands
description: 'Add, modify, or remove shell CLI commands in GuitarAcc basestation firmware. Use when: adding new commands, changing command behavior, renaming commands, removing commands, updating command arguments, or managing Zephyr shell command structure.'
argument-hint: 'describe the CLI command change (add/modify/remove)'
---

# GuitarAcc CLI Command Management

Workflow for managing Zephyr shell commands in the GuitarAcc basestation firmware.

## When to Use

- Adding new shell commands to the CLI interface
- Modifying existing command behavior or arguments
- Renaming commands (e.g., `pipeline_monitor` → `monitor`)
- Removing deprecated commands
- Changing command structure (standalone → subcommand or vice versa)
- Updating help text or command descriptions

## Command Architecture

### File Structure

Commands are implemented in:
- **`basestation/src/ui_interface_shell.c`** - Command handlers and registrations
- **`basestation/src/ui_interface.h`** - Public API declarations (if backend functions needed)
- **`basestation/src/main.c`** - Backend implementation (for data access/processing)

### Zephyr Shell Command Types

1. **Standalone Command**: `SHELL_CMD_REGISTER()`
   ```c
   SHELL_CMD_REGISTER(status, NULL, "Show system status", cmd_status);
   ```

2. **Standalone with Arguments**: `SHELL_CMD_ARG_REGISTER()`
   ```c
   SHELL_CMD_ARG_REGISTER(monitor, NULL, "Show pipeline snapshot [json]", cmd_monitor, 1, 1);
   //                     ^name   ^subs  ^help                           ^handler   ^min ^max_optional
   ```

3. **Command Group with Subcommands**: `SHELL_STATIC_SUBCMD_SET_CREATE()`
   ```c
   SHELL_STATIC_SUBCMD_SET_CREATE(sub_pipeline,
       SHELL_CMD(set, NULL, "Configure pipeline", cmd_pipeline_set),
       SHELL_CMD(show, NULL, "Show configuration", cmd_pipeline_show),
       SHELL_SUBCMD_SET_END
   );
   SHELL_CMD_REGISTER(pipeline, &sub_pipeline, "Pipeline commands", NULL);
   ```

## Workflow

### Step 1: Identify the Change

Determine what type of change is needed:

**Adding a command:**
- What does it do?
- Standalone or part of a command group?
- What arguments does it take?
- Does it need backend functions in main.c?
- What help text should users see?

**Modifying a command:**
- What behavior changes?
- Are arguments changing (count, type, meaning)?
- Does the backend need updates?
- Does help text need updates?

**Renaming a command:**
- New name (keep it short for CLI use)
- Update all references (handler function, registration, help text)

**Removing a command:**
- Is it safe to remove (no dependencies)?
- Remove or comment out?
- Remove from help documentation

### Step 2: Locate Existing Code

**Find command registration** (in ui_interface_shell.c, usually bottom of file):
```bash
grep -n "SHELL_CMD.*REGISTER" basestation/src/ui_interface_shell.c
grep -n "SHELL_STATIC_SUBCMD_SET_CREATE" basestation/src/ui_interface_shell.c
```

**Find command handler** (search for function name):
```bash
grep -n "cmd_<command_name>" basestation/src/ui_interface_shell.c
```

### Step 3: Implement the Change

#### For Adding a New Command

1. **Define handler function** in ui_interface_shell.c:
   ```c
   static int cmd_mycommand(const struct shell *sh, size_t argc, char **argv)
   {
       ARG_UNUSED(argc);
       ARG_UNUSED(argv);
       
       // Validate arguments
       if (argc < 2) {
           shell_error(sh, "Usage: mycommand <arg>");
           return -1;
       }
       
       // Implementation here
       
       shell_print(sh, "Success!");
       return 0;
   }
   ```

2. **Register the command** (bottom of ui_interface_shell.c):
   ```c
   SHELL_CMD_ARG_REGISTER(mycommand, NULL, "Description", cmd_mycommand, 2, 0);
   ```

3. **Add backend functions** (if needed in main.c):
   - Add function prototype to ui_interface.h
   - Implement in main.c with access to internal state

#### For Modifying a Command

1. Update the handler function logic
2. If arguments changed, update the registration (min/max args)
3. Update help text if behavior changed
4. Update backend functions if needed

#### For Renaming a Command

1. Rename the handler function: `cmd_oldname` → `cmd_newname`
2. Update the registration: change command name string
3. Search for any references to the old name
4. Update help text/documentation

#### For Removing a Command

1. Comment out or delete the handler function
2. Comment out or delete the registration
3. Remove any backend functions if they're no longer used
4. Check for dependencies (other commands calling this one)

### Step 4: Update Help Text and Documentation

**Every command change requires help text updates:**

1. **Update command registration help string** (in ui_interface_shell.c):
   ```c
   SHELL_CMD_ARG_REGISTER(monitor, NULL, "Show pipeline snapshot [json]", cmd_monitor, 1, 1);
   //                                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   //                                     Keep concise, describe what it does
   ```

2. **Add detailed help in command handler** (for complex commands):
   ```c
   static int cmd_mycommand(const struct shell *sh, size_t argc, char **argv)
   {
       if (argc < 2) {
           shell_error(sh, "Usage: mycommand <arg1> [arg2]");
           shell_print(sh, "");
           shell_print(sh, "Description of what this command does");
           shell_print(sh, "");
           shell_print(sh, "Arguments:");
           shell_print(sh, "  arg1 - Description of required argument");
           shell_print(sh, "  arg2 - Description of optional argument (default: value)");
           shell_print(sh, "");
           shell_print(sh, "Examples:");
           shell_print(sh, "  mycommand 10       # Example usage");
           shell_print(sh, "  mycommand 10 20    # With optional argument");
           return -1;
       }
       // ... rest of implementation
   }
   ```

3. **Update any related documentation** (if exists):
   - `basestation/UI_INTERFACE.md` - User interface documentation
   - `basestation/ARCHITECTURE.md` - System architecture if command affects it
   - Comments in the code explaining behavior

### Step 5: Update Firmware Version

**CRITICAL: Always increment the minor version after any CLI change** in `basestation/CMakeLists.txt`:

```cmake
set(FIRMWARE_VERSION_MINOR 4)  # Was 3, NOW 4 - increment for this change
```

**Why this matters:**
- Allows verification that new firmware was actually flashed
- Version shown in `status` command confirms you're running the latest build
- Prevents confusion when testing ("Am I running old or new firmware?")

**Version increment rules:**
- **MINOR version** - Any CLI command change (add/modify/rename/remove) → **INCREMENT THIS**
- MAJOR version - Breaking changes, incompatible API changes
- PATCH version - Bug fixes only (no functional changes)

### Step 6: Build and Test

1. **Build** using nRF Connect extension or:
   ```bash
   cd basestation
   west build -b nrf5340_audio_dk/nrf5340/cpuapp
   ```

2. **Flash**:
   ```bash
   west flash
   ```

3. **Verify version** after flashing:
   ```bash
   GuitarAcc:~$ status
   Firmware Version: 1.3.0  # Should match CMakeLists.txt
   ```

4. **Test the command**:
   ```bash
   GuitarAcc:~$ mycommand arg1
   Success!
   ```

5. **Test help text**:
   ```bash
   GuitarAcc:~$ help mycommand
   # Or just type the command without arguments to see usage
   GuitarAcc:~$ mycommand
   ```

### Step 7: Handle Common Issues

**Command not found:**
- Check registration is not commented out
- Verify firmware was flashed (check version with `status`)
- Rebuild from clean: `west build -t pristine && west build`

**Wrong parameter count error:**
- Check `argc` in handler matches registration (min, max_optional)
- Registration: `cmd_handler, min_required, max_optional`

**Float printf warnings:**
- Cast floats to double: `(double)float_var`
- Or ensure `CONFIG_CBPRINTF_FP_SUPPORT=y` in prj.conf

## Examples from GuitarAcc

### Example 1: Renaming `pipeline_monitor` to `monitor`

**Change:**
- Shorter command name for ease of use
- Snapshot-based (no enable/disable)

**Implementation:**
1. Renamed handler: `cmd_pipeline_monitor` → `cmd_monitor`
2. Updated registration:
   ```c
   SHELL_CMD_ARG_REGISTER(monitor, NULL, "Show pipeline snapshot [json]", cmd_monitor, 1, 1);
   ```
3. Updated help text: Changed description to be more concise
4. **Incremented version:** `1.2.0 → 1.3.0` in CMakeLists.txt
5. Built and flashed
6. Verified: `status` shows `1.3.0`, and `monitor` + `monitor json` work

### Example 2: Converting Streaming to Snapshot Monitoring

**Change:**
- From continuous monitoring (enable/disable) to instant snapshot queries
- Simpler user interface

**Implementation:**
1. Modified backend (main.c):
   - Changed from `pipeline_monitor_format` state variable to `last_pipeline_data` snapshot
   - Removed streaming functions, added `get_pipeline_snapshot()`
2. Modified command handler:
   - Removed enable/disable/json arguments
   - Added optional `json` argument
   - Changed to call `get_pipeline_snapshot()` and display immediately
   - Updated help text with usage examples for both formats
3. Updated registration to accept 1 required, 1 optional arg
4. **Incremented version:** in CMakeLists.txt
5. Tested both formats (human-readable and JSON)
6. Verified version with `status` command before testing

## Command Design Guidelines

1. **Keep names short** - users type these frequently (`monitor` not `pipeline_monitor`)
2. **Use subcommands for groups** - related commands under one parent (`pipeline set`, `pipeline show`)
3. **Consistent help text** - describe what it does, not how to use it (usage shown automatically)
4. **Provide detailed help** - for complex commands, show examples in the error/help output
5. **Return 0 for success, -1 for error** - standard shell convention
6. **Validate arguments early** - check count and format before processing
7. **Use shell_print for success, shell_error for failures** - provides color coding
8. **JSON output for automation** - consider adding JSON format for script-friendly output
9. **Always increment FIRMWARE_VERSION_MINOR** - for ANY command change (add/modify/remove/rename)

## File Locations Reference

```
basestation/
├── src/
│   ├── ui_interface_shell.c    # Command handlers and registrations (PRIMARY)
│   ├── ui_interface.h          # Public API for backend functions
│   ├── ui_interface.c          # (Legacy) Some UI functions
│   └── main.c                  # Backend data access and processing
├── CMakeLists.txt              # Version number (increment for changes)
└── prj.conf                    # Build configuration (e.g., float support)
```

## Quick Reference: Shell Macros

- `SHELL_CMD_REGISTER(name, subcommands, help, handler)` - Register standalone command
- `SHELL_CMD_ARG_REGISTER(name, subcommands, help, handler, min, opt)` - With argument count validation
- `SHELL_STATIC_SUBCMD_SET_CREATE(group_name, ...)` - Define subcommand group
- `SHELL_CMD(name, subs, help, handler)` - Subcommand within a group
- `SHELL_SUBCMD_SET_END` - Terminate subcommand list
- `ARG_UNUSED(var)` - Suppress unused parameter warnings
- `shell_print(sh, fmt, ...)` - Print output (like printf)
- `shell_error(sh, fmt, ...)` - Print error message (colored red)

## CLI Command Change Checklist

Use this checklist for **every** command change:

- [ ] **Identify change type** - Add/modify/rename/remove
- [ ] **Locate existing code** - Find handler and registration
- [ ] **Implement change** - Modify handler function and/or registration
- [ ] **Update help text** - Registration string AND detailed help in handler
- [ ] **Update backend** - If needed (main.c, ui_interface.h)
- [ ] **INCREMENT MINOR VERSION** - Edit CMakeLists.txt (e.g., `1.3.0` → `1.4.0`)
- [ ] **Build firmware** - `west build` or nRF Connect extension
- [ ] **Flash firmware** - `west flash` or nRF Connect extension
- [ ] **Verify version** - Run `status` command, check version matches CMakeLists.txt
- [ ] **Test command** - Run the command with various arguments
- [ ] **Test help** - Run `help <command>` or command without args
- [ ] **Test edge cases** - Invalid args, boundary conditions

**Critical reminder:** If `status` shows old version after flashing, the new firmware didn't flash properly. Rebuild from clean or check flash process.

## Additional Resources

- [Zephyr Shell Documentation](https://docs.zephyrproject.org/latest/services/shell/index.html)
- [basestation/VERSIONING.md](../../basestation/VERSIONING.md) - Version management
- [basestation/UI_INTERFACE.md](../../basestation/UI_INTERFACE.md) - UI design patterns
