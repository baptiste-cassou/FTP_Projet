#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(pwd)
HOST="127.0.0.1"
FILE_NAME="integrity_test.bin"
SIZE_MB="2"
SERVER_FILE="$ROOT_DIR/data_server/$FILE_NAME"
CLIENT_FILE="$ROOT_DIR/data_client/$FILE_NAME"
SERVER_LOG="$ROOT_DIR/logs/server_test.log"
CLIENT_LOG="$ROOT_DIR/logs/client_output.log"
STARTED_SERVER=0

cleanup() {
  if [[ "$STARTED_SERVER" -eq 1 ]] && [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill -SIGINT "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

echo "[1/6] Build binaries"
make -C "$ROOT_DIR" all >/dev/null

echo "[2/6] Generate test file ($SIZE_MB MiB): $SERVER_FILE"
mkdir -p "$ROOT_DIR/data_server" "$ROOT_DIR/data_client"
dd if=/dev/urandom of="$SERVER_FILE" bs=1M count="$SIZE_MB" status=none
rm -f "$CLIENT_FILE"
head -c 700000 "$SERVER_FILE" > "$CLIENT_FILE"
stat -c "server file: %s bytes" "$SERVER_FILE"
stat -c "client file: %s bytes" "$CLIENT_FILE"
echo "[3/6] Start server"
"$ROOT_DIR/bin/serverFTP" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 1
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  if grep -q "Address already in use" "$SERVER_LOG"; then
    echo "Server already running on port 2121, reusing existing instance"
  else
    echo "Server failed to start. Log: $SERVER_LOG"
    tail -n 50 "$SERVER_LOG" || true
    exit 1
  fi
else
  STARTED_SERVER=1
fi

echo "[4/6] Download with client"
printf "get %s\nbye\n" "$FILE_NAME" | "$ROOT_DIR/bin/clientFTP" "$HOST" > "$CLIENT_LOG" 2>&1

echo "[5/6] Verify integrity"
SERVER_HASH="$(sha256sum "$SERVER_FILE" |cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum "$CLIENT_FILE" |cut -d ' ' -f1)"
SERVER_SIZE="$(wc -c < "$SERVER_FILE")"
CLIENT_SIZE="$(wc -c < "$CLIENT_FILE")"

echo "Server size: $SERVER_SIZE"
echo "Client size: $CLIENT_SIZE"
echo "Server sha256: $SERVER_HASH"
echo "Client sha256: $CLIENT_HASH"

if [[ "$SERVER_SIZE" != "$CLIENT_SIZE" ]]; then
  echo "FAIL: file sizes differ"
  exit 1
fi

if [[ "$SERVER_HASH" != "$CLIENT_HASH" ]]; then
  echo "FAIL: file hashes differ"
  exit 1
fi

echo "[6/6] PASS: integrity verified"
rm -f "$SERVER_FILE" "$CLIENT_FILE"