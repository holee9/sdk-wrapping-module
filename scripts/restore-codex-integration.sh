#!/bin/bash
# Codex Integration Restore Script
# Run this script to restore Codex integration from backup

echo "==================================="
echo "Codex Integration Restore"
echo "==================================="
echo ""

BACKUP_DIR="$HOME/.codex-backup"
MCP_CONFIG="C:/Users/user/.mcp.json"
CODEX_EXE="C:/Users/user/.vscode/extensions/openai.chatgpt-0.4.74-win32-x64/bin/windows-x86_64/codex.exe"

# Check if backup exists
if [ ! -d "$BACKUP_DIR" ]; then
    echo "Error: Backup directory not found: $BACKUP_DIR"
    exit 1
fi

echo "[1/4] Restoring MCP configuration..."
if [ -f "$BACKUP_DIR/mcp.json.backup" ]; then
    cp "$BACKUP_DIR/mcp.json.backup" "$MCP_CONFIG"
    echo "✓ MCP configuration restored"
else
    echo "✗ MCP backup not found"
fi

echo ""
echo "[2/4] Restoring Codex configuration..."
if [ -f "$BACKUP_DIR/config.toml.backup" ]; then
    mkdir -p ~/.codex
    cp "$BACKUP_DIR/config.toml.backup" ~/.codex/config.toml
    echo "✓ Codex configuration restored"
else
    echo "✗ Codex backup not found"
fi

echo ""
echo "[3/4] Verifying Codex login..."
LOGIN_STATUS=$("$CODEX_EXE" login status 2>&1)
if echo "$LOGIN_STATUS" | grep -q "Logged in"; then
    echo "✓ Codex is logged in"
else
    echo "⚠ Codex not logged in. Run: codex login"
fi

echo ""
echo "[4/4] Testing Codex command..."
echo "test" | "$CODEX_EXE" exec --json --output-last-message /tmp/restore_test.json >/dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✓ Codex command test passed"
    rm -f /tmp/restore_test.json
else
    echo "✗ Codex command test failed"
fi

echo ""
echo "==================================="
echo "Restore completed"
echo "==================================="
echo ""
echo "Next steps:"
echo "1. Restart Claude Code to reload MCP configuration"
echo "2. Run ./scripts/verify-codex-integration.sh to verify"
