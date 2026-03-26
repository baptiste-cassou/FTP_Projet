#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
HOST="127.0.0.1"
SLAVE1_ID=1
SLAVE2_ID=2
SLAVE1_PORT=3001
LOG_DIR="$ROOT_DIR/logs"

TEXT_FILE="demo_q1_q7.txt"
BIN_FILE="demo_q8_q9.bin"
RESUME_FILE="demo_q10_resume.bin"

SERVER_TEXT_PATH="$ROOT_DIR/data_server/$TEXT_FILE"
SERVER_BIN_PATH="$ROOT_DIR/data_server/$BIN_FILE"
SERVER_RESUME_PATH="$ROOT_DIR/data_server/$RESUME_FILE"

CLIENT_TEXT_PATH="$ROOT_DIR/data_client/$TEXT_FILE"
CLIENT_BIN_PATH="$ROOT_DIR/data_client/$BIN_FILE"
CLIENT_RESUME_PATH="$ROOT_DIR/data_client/$RESUME_FILE"

SLAVE1_LOG="$LOG_DIR/demo_slave1.log"
SLAVE2_LOG="$LOG_DIR/demo_slave2.log"
MASTER_LOG="$LOG_DIR/demo_master.log"
CLIENT_MULTI_LOG="$LOG_DIR/demo_client_q1_q9.log"
CLIENT_RESUME_LOG="$LOG_DIR/demo_client_q10.log"

PIDS=()

cleanup() {
  local pid

  for pid in "${PIDS[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill -SIGINT "$pid" 2>/dev/null || true
    fi
  done

  for pid in "${PIDS[@]:-}"; do
    wait "$pid" 2>/dev/null || true
  done

  rm -f "$SERVER_TEXT_PATH" "$SERVER_BIN_PATH" "$SERVER_RESUME_PATH"
  rm -f "$CLIENT_TEXT_PATH" "$CLIENT_BIN_PATH" "$CLIENT_RESUME_PATH"
}
trap cleanup EXIT

wait_for_log() {
  local file="$1"
  local pattern="$2"
  local timeout_seconds="${3:-10}"
  local waited=0

  while (( waited < timeout_seconds * 10 )); do
    if [[ -f "$file" ]] && grep -q "$pattern" "$file"; then
      return 0
    fi
    sleep 0.1
    waited=$((waited + 1))
  done

  echo "Timeout while waiting for pattern '$pattern' in $file" >&2
  if [[ -f "$file" ]]; then
    echo "--- $file ---" >&2
    cat "$file" >&2
  fi
  return 1
}

echo "[1/8] Build project"
mkdir -p "$LOG_DIR" "$ROOT_DIR/data_server" "$ROOT_DIR/data_client"
make -C "$ROOT_DIR" clean >/dev/null
make -C "$ROOT_DIR" all >/dev/null

echo "[2/8] Prepare demonstration files"
printf 'Demonstration FTP Q1-Q7\n' > "$SERVER_TEXT_PATH"
dd if=/dev/urandom of="$SERVER_BIN_PATH" bs=1M count=1 status=none
dd if=/dev/urandom of="$SERVER_RESUME_PATH" bs=1M count=2 status=none
rm -f "$CLIENT_TEXT_PATH" "$CLIENT_BIN_PATH" "$CLIENT_RESUME_PATH"

echo "[3/8] Start slave 1 and slave 2"
stdbuf -oL -eL "$ROOT_DIR/bin/serverFTP" "$SLAVE1_ID" >"$SLAVE1_LOG" 2>&1 &
PIDS+=("$!")
stdbuf -oL -eL "$ROOT_DIR/bin/serverFTP" "$SLAVE2_ID" >"$SLAVE2_LOG" 2>&1 &
PIDS+=("$!")

wait_for_log "$SLAVE1_LOG" "waiting for master on control port"
wait_for_log "$SLAVE2_LOG" "waiting for master on control port"

echo "[4/8] Start master and validate Q11-Q12 registration"
stdbuf -oL -eL "$ROOT_DIR/bin/masterFTP" >"$MASTER_LOG" 2>&1 &
PIDS+=("$!")

wait_for_log "$MASTER_LOG" "slave 1 registered"
wait_for_log "$MASTER_LOG" "slave 2 registered"
wait_for_log "$MASTER_LOG" "listening on port 2121 with 2 registered slaves"
wait_for_log "$SLAVE1_LOG" "listening on client port 3001"
wait_for_log "$SLAVE2_LOG" "listening on client port 3002"

echo "Q11/Q12 OK:"
cat "$MASTER_LOG"

echo "[5/8] Demonstrate Q1-Q9 using direct connection to slave 1"
echo "Note: direct slave connection is used here because client redirection belongs to Q13."
printf "get %s\nget %s\nbye\n" "$TEXT_FILE" "$BIN_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$CLIENT_MULTI_LOG" 2>&1

grep -q "Transfer successfully complete." "$CLIENT_MULTI_LOG"
test -f "$CLIENT_TEXT_PATH"
test -f "$CLIENT_BIN_PATH"

echo "Client session Q1-Q9:"
cat "$CLIENT_MULTI_LOG"

echo "[6/8] Demonstrate Q10 resume"
head -c 700000 "$SERVER_RESUME_PATH" > "$CLIENT_RESUME_PATH"
printf "get %s\nbye\n" "$RESUME_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$CLIENT_RESUME_LOG" 2>&1

SERVER_HASH="$(sha256sum "$SERVER_RESUME_PATH" | cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum "$CLIENT_RESUME_PATH" | cut -d ' ' -f1)"

if [[ "$SERVER_HASH" != "$CLIENT_HASH" ]]; then
  echo "Resume verification failed" >&2
  echo "server hash: $SERVER_HASH" >&2
  echo "client hash: $CLIENT_HASH" >&2
  exit 1
fi

echo "Client session Q10:"
cat "$CLIENT_RESUME_LOG"

echo "[7/8] Summary"
echo "- Q1-Q7: request/response structures, client/server skeleton, SIGINT cleanup, directories, GET"
echo "- Q8: transfer by blocks demonstrated on binary files"
echo "- Q9: multiple commands in a single client session"
echo "- Q10: resume verified by matching SHA256 after partial local file"
echo "- Q11: static slave count and dedicated ports"
echo "- Q12: master registered 2 slaves before listening on 2121"

echo "[8/8] PASS"
