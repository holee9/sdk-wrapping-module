#!/bin/bash
# Codex Integration Verification Script
# Run this script to verify Claude Code + Codex integration status

echo "==================================="
echo "Codex Integration Verification"
echo "==================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
PASS=0
FAIL=0

check_pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((PASS++))
}

check_fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((FAIL++))
}

check_warn() {
    echo -e "${YELLOW}⚠ WARN${NC}: $1"
}

# 1. Check Codex Extension
echo "[1/6] Checking Codex Extension..."
CODEX_PATH="C:/Users/user/.vscode/extensions/openai.chatgpt-0.4.74-win32-x64"
if [ -d "$CODEX_PATH" ]; then
    check_pass "Codex extension found at $CODEX_PATH"
else
    check_fail "Codex extension not found"
fi

# 2. Check Codex Executable
echo ""
echo "[2/6] Checking Codex Executable..."
CODEX_EXE="$CODEX_PATH/bin/windows-x86_64/codex.exe"
if [ -f "$CODEX_EXE" ]; then
    check_pass "Codex executable found"
else
    check_fail "Codex executable not found"
fi

# 3. Check Codex Login Status
echo ""
echo "[3/6] Checking Codex Login Status..."
LOGIN_STATUS=$("$CODEX_EXE" login status 2>&1)
if echo "$LOGIN_STATUS" | grep -q "Logged in"; then
    check_pass "Codex is logged in"
else
    check_fail "Codex not logged in. Run: codex login"
fi

# 4. Check MCP Configuration
echo ""
echo "[4/6] Checking MCP Configuration..."
MCP_CONFIG="C:/Users/user/.mcp.json"
if [ -f "$MCP_CONFIG" ]; then
    if grep -q "codex" "$MCP_CONFIG"; then
        check_pass "Codex MCP server configured in .mcp.json"
    else
        check_fail "Codex not configured in .mcp.json"
    fi
else
    check_fail ".mcp.json not found"
fi

# 5. Check Backup Files
echo ""
echo "[5/6] Checking Backup Files..."
BACKUP_DIR="$HOME/.codex-backup"
if [ -d "$BACKUP_DIR" ]; then
    if [ -f "$BACKUP_DIR/mcp.json.backup" ] && [ -f "$BACKUP_DIR/config.toml.backup" ]; then
        check_pass "Backup files exist"
    else
        check_warn "Some backup files missing"
    fi
else
    check_warn "Backup directory not found"
fi

# 6. Test Codex Command
echo ""
echo "[6/6] Testing Codex Command..."
TEST_RESULT=$(echo "What is 2+2?" | "$CODEX_EXE" exec --json --output-last-message /tmp/codex_test.json 2>&1)
if [ -f "/tmp/codex_test.json" ]; then
    check_pass "Codex command execution successful"
    rm -f /tmp/codex_test.json
else
    check_fail "Codex command execution failed"
fi

# Summary
echo ""
echo "==================================="
echo "Summary: $PASS passed, $FAIL failed"
echo "==================================="

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All checks passed! Integration is working.${NC}"
    exit 0
else
    echo -e "${RED}Some checks failed. Please review and fix issues.${NC}"
    exit 1
fi
