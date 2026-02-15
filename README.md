# sdk-wrapping-module

## Claude Code + Codex Integration Guide

This project has verified integration between Claude Code and OpenAI's Codex extension for task delegation and result retrieval.

### Prerequisites

- **Claude Code**: Installed and configured
- **Codex Extension**: OpenAI ChatGPT VSCode extension (`openai.chatgpt`)
- **ChatGPT Account**: Logged in (Plus, Pro, Business, Edu, or Enterprise plan)

### Installation Paths

| Component | Path |
|-----------|------|
| Codex Extension | `C:\Users\user\.vscode\extensions\openai.chatgpt-0.4.74-win32-x64` |
| Codex Executable | `bin\windows-x86_64\codex.exe` |
| MCP Config | `C:\Users\user\.mcp.json` |
| Codex Config | `C:\Users\user\.codex\config.toml` |

### Configuration

#### Step 1: Verify Codex Login

```bash
"C:\Users\user\.vscode\extensions\openai.chatgpt-0.4.74-win32-x64\bin\windows-x86_64\codex.exe" login status
```

Expected output: `Logged in using ChatGPT`

#### Step 2: Update MCP Configuration

Edit `C:\Users\user\.mcp.json` and add the Codex MCP server:

```json
{
  "$schema": "https://raw.githubusercontent.com/anthropics/claude-code/main/.mcp.schema.json",
  "mcpServers": {
    "codex": {
      "$comment": "OpenAI Codex - Claude Code integration for task delegation",
      "command": "C:\\Users\\user\\.vscode\\extensions\\openai.chatgpt-0.4.74-win32-x64\\bin\\windows-x86_64\\codex.exe",
      "args": ["mcp-server"]
    }
  },
  "staggeredStartup": {
    "enabled": true,
    "delayMs": 500,
    "connectionTimeout": 15000
  }
}
```

### Usage

#### Method 1: Direct CLI Command (Immediate Use)

Execute Codex tasks from Claude Code using Bash tool:

```bash
# Basic task delegation
"C:\Users\user\.vscode\extensions\openai.chatgpt-0.4.74-win32-x64\bin\windows-x86_64\codex.exe" \
  exec --json --output-last-message /tmp/codex_result.json "[YOUR_TASK_HERE]"

# Example: Summarize README
codex exec --json --output-last-message /tmp/result.json "Summarize the README.md file"

# Example: Code review
codex exec review
```

#### Method 2: MCP Tools (After Session Restart)

After restarting Claude Code, Codex MCP tools will be available:

1. Restart Claude Code session
2. MCP tools auto-load from configuration
3. Use `ToolSearch` to discover available tools: `mcp__codex__*`
4. Call tools directly for task delegation

### Verification

Test the integration:

```bash
# Run test task
"C:\Users\user\.vscode\extensions\openai.chatgpt-0.4.74-win32-x64\bin\windows-x86_64\codex.exe" \
  exec --json --output-last-message /tmp/test_result.json "What is 2+2?"

# Check result
cat /tmp/test_result.json
```

Expected result: A JSON response with the answer.

### Troubleshooting

| Issue | Solution |
|-------|----------|
| `codex: command not found` | Use full path to codex.exe |
| `Not logged in` | Run `codex login` and authenticate with ChatGPT |
| MCP tools not available | Restart Claude Code session |
| Path too long error | Use Windows short path format or wrap in quotes |

### Process Verification

Verify Codex processes are running:

```bash
# Check for Codex and MCP server processes
ps -aW | grep -E "(codex|mcp-server)"
```

Expected output: Active codex.exe and mcp-server-windows-x64.exe processes.

### Maintenance

#### Automated Scripts

The project includes helper scripts for maintenance:

```bash
# Verify integration status (run periodically)
./scripts/verify-codex-integration.sh

# Restore integration from backup (if something breaks)
./scripts/restore-codex-integration.sh
```

**Verification Script Output:**
```
===================================
Codex Integration Verification
===================================

[1/6] Checking Codex Extension...
✓ PASS: Codex extension found at ...
[2/6] Checking Codex Executable...
✓ PASS: Codex executable found
...

Summary: 6 passed, 0 failed
===================================
All checks passed! Integration is working.
```

#### Update Paths After Extension Update

When Codex extension updates, version number in path changes. Update `.mcp.json`:

```bash
# Check installed extension versions
ls "C:\Users\user\.vscode\extensions\" | grep chatgpt

# Update path in .mcp.json to new version
```

#### Session ID Tracking

Current session integration verified:
- Session ID format: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`
- Codex MCP server: stdio transport
- Communication: JSONL format over stdin/stdout

---

## Project Structure

```
sdk-wrapping-module/
├── README.md                           # This file
├── scripts/                            # Maintenance scripts
│   ├── verify-codex-integration.sh     # Verify integration status
│   └── restore-codex-integration.sh    # Restore from backup
├── .mcp.json                           # MCP server configuration
├── .codex/                             # Codex configuration
│   └── config.toml                     # Codex settings
└── [project files]
```

### Backup Location

Configuration backups are stored at:
```
~/.codex-backup/
├── mcp.json.backup        # MCP configuration backup
├── config.toml.backup     # Codex configuration backup
└── INTEGRATION_STATUS.md  # Integration status documentation
```

---

**Last Updated**: 2026-02-16
**Integration Status**: ✅ Verified and Working