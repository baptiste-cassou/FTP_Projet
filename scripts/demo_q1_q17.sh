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
RECONNECT_FILE="demo_q14_reconnect.bin"
UPLOAD_FILE="demo_q16_upload.txt"
LIST_SEED_FILE="demo_q15_seed.txt"

SERVER_TEXT_PATH="$ROOT_DIR/data_server/$TEXT_FILE"
SERVER_BIN_PATH="$ROOT_DIR/data_server/$BIN_FILE"
SERVER_RESUME_PATH="$ROOT_DIR/data_server/$RESUME_FILE"
SERVER_RECONNECT_PATH="$ROOT_DIR/data_server/$RECONNECT_FILE"
SERVER_UPLOAD_PATH="$ROOT_DIR/data_server/$UPLOAD_FILE"
SERVER_LIST_SEED_PATH="$ROOT_DIR/data_server/$LIST_SEED_FILE"

CLIENT_TEXT_PATH="$ROOT_DIR/data_client/$TEXT_FILE"
CLIENT_BIN_PATH="$ROOT_DIR/data_client/$BIN_FILE"
CLIENT_RESUME_PATH="$ROOT_DIR/data_client/$RESUME_FILE"
CLIENT_RECONNECT_PATH="$ROOT_DIR/data_client/$RECONNECT_FILE"
CLIENT_UPLOAD_PATH="$ROOT_DIR/data_client/$UPLOAD_FILE"

SLAVE1_LOG="$LOG_DIR/demo_slave1.log"
SLAVE2_LOG="$LOG_DIR/demo_slave2.log"
MASTER_LOG="$LOG_DIR/demo_master.log"
CLIENT_MULTI_LOG="$LOG_DIR/demo_client_q1_q9.log"
CLIENT_RESUME_LOG="$LOG_DIR/demo_client_q10.log"
CLIENT_Q13_LOG="$LOG_DIR/demo_client_q13.log"
CLIENT_Q14_LOG="$LOG_DIR/demo_client_q14.log"
CLIENT_Q15_Q17_LOG="$LOG_DIR/demo_client_q15_q17.log"
CLIENT_Q16_RM_LOG="$LOG_DIR/demo_client_q16_rm.log"

PIDS=()

if [[ -t 1 ]]; then
  COLOR_RESET=$'\033[0m'
  COLOR_MASTER=$'\033[1;35m'
  COLOR_SLAVE1=$'\033[1;32m'
  COLOR_SLAVE2=$'\033[1;36m'
  COLOR_CLIENT=$'\033[1;33m'
else
  COLOR_RESET=''
  COLOR_MASTER=''
  COLOR_SLAVE1=''
  COLOR_SLAVE2=''
  COLOR_CLIENT=''
fi

prefix_live_output() {
  local label="$1"
  local color="$2"

  awk -v label="[$label]" -v color="$color" -v reset="$COLOR_RESET" '
    {
      print color label reset " " $0
      fflush(stdout)
    }
  '
}

print_live_log_path() {
  echo "    Logs directory: $LOG_DIR"
}

stop_cluster() {
  local pid

  for pid in "${PIDS[@]:-}"; do
    kill -INT "$pid" 2>/dev/null || true
  done

  pkill -INT serverFTP 2>/dev/null || true
  pkill -INT masterFTP 2>/dev/null || true

  for pid in "${PIDS[@]:-}"; do
    wait "$pid" 2>/dev/null || true
  done

  PIDS=()
}

cleanup() {
  stop_cluster

  rm -f "$SERVER_TEXT_PATH" "$SERVER_BIN_PATH" "$SERVER_RESUME_PATH" \
        "$SERVER_RECONNECT_PATH" "$SERVER_UPLOAD_PATH" "$SERVER_LIST_SEED_PATH"
  rm -f "$CLIENT_TEXT_PATH" "$CLIENT_BIN_PATH" "$CLIENT_RESUME_PATH" \
        "$CLIENT_RECONNECT_PATH" "$CLIENT_UPLOAD_PATH"
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

start_cluster() {
  : > "$SLAVE1_LOG"
  : > "$SLAVE2_LOG"
  : > "$MASTER_LOG"

  stdbuf -oL -eL "$ROOT_DIR/bin/serverFTP" "$SLAVE1_ID" \
    > >(tee "$SLAVE1_LOG" | prefix_live_output "slave1" "$COLOR_SLAVE1") 2>&1 &
  PIDS+=("$!")
  stdbuf -oL -eL "$ROOT_DIR/bin/serverFTP" "$SLAVE2_ID" \
    > >(tee "$SLAVE2_LOG" | prefix_live_output "slave2" "$COLOR_SLAVE2") 2>&1 &
  PIDS+=("$!")

  wait_for_log "$SLAVE1_LOG" "waiting for master on control port"
  wait_for_log "$SLAVE2_LOG" "waiting for master on control port"

  stdbuf -oL -eL "$ROOT_DIR/bin/masterFTP" \
    > >(tee "$MASTER_LOG" | prefix_live_output "master" "$COLOR_MASTER") 2>&1 &
  PIDS+=("$!")

  wait_for_log "$MASTER_LOG" "slave 1 registered"
  wait_for_log "$MASTER_LOG" "slave 2 registered"
  wait_for_log "$MASTER_LOG" "listening on port 2121 with 2 registered slaves"
  wait_for_log "$SLAVE1_LOG" "listening on client port 3001"
  wait_for_log "$SLAVE2_LOG" "listening on client port 3002"
}

echo "==========================================="
echo "[1/12] Build project"
mkdir -p "$LOG_DIR" "$ROOT_DIR/data_server" "$ROOT_DIR/data_client"
print_live_log_path
make -C "$ROOT_DIR" clean
make -C "$ROOT_DIR" all

echo "==========================================="
echo "[2/12] Prepare demonstration files"
printf 'Demonstration FTP Q1-Q7\n' > "$SERVER_TEXT_PATH"
printf 'Visible in LS\n' > "$SERVER_LIST_SEED_PATH"
dd if=/dev/urandom of="$SERVER_BIN_PATH" bs=1M count=1 status=none
dd if=/dev/urandom of="$SERVER_RESUME_PATH" bs=1M count=2 status=none
dd if=/dev/urandom of="$SERVER_RECONNECT_PATH" bs=1M count=256 status=none
printf 'upload from client\n' > "$CLIENT_UPLOAD_PATH"
rm -f "$CLIENT_TEXT_PATH" "$CLIENT_BIN_PATH" "$CLIENT_RESUME_PATH" "$CLIENT_RECONNECT_PATH" "$SERVER_UPLOAD_PATH"

echo "==========================================="
echo "[3/12] Start cluster and validate Q11-Q12"
start_cluster

echo "==========================================="
echo "[4/12] Demonstrate Q1-Q9 using direct connection to slave 1"
printf "get %s\nget %s\nbye\n" "$TEXT_FILE" "$BIN_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" 2>&1 \
  | tee "$CLIENT_MULTI_LOG" \
  | prefix_live_output "client-q1-q9" "$COLOR_CLIENT"

grep -q "Transfer successfully complete." "$CLIENT_MULTI_LOG"
test -f "$CLIENT_TEXT_PATH"
test -f "$CLIENT_BIN_PATH"

echo "==========================================="
echo "[5/12] Demonstrate Q10 resume"
head -c 700000 "$SERVER_RESUME_PATH" > "$CLIENT_RESUME_PATH"
printf "get %s\nbye\n" "$RESUME_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" 2>&1 \
  | tee "$CLIENT_RESUME_LOG" \
  | prefix_live_output "client-q10" "$COLOR_CLIENT"

SERVER_HASH="$(sha256sum "$SERVER_RESUME_PATH" | cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum "$CLIENT_RESUME_PATH" | cut -d ' ' -f1)"
[[ "$SERVER_HASH" == "$CLIENT_HASH" ]]

echo "==========================================="
echo "[6/12] Demonstrate Q10 server robustness after abrupt client disconnect"
rm -f "$CLIENT_RECONNECT_PATH"
echo "    Sending a raw GET request, then cutting the socket abruptly"
python3 - <<'PY'
import ctypes
import socket
import struct

FTP_MAX_FILENAME = 256
FTP_MAX_LOGIN = 32
FTP_MAX_PASSWORD = 64

class Request(ctypes.Structure):
    _fields_ = [
        ("version", ctypes.c_uint32),
        ("type", ctypes.c_uint32),
        ("offset", ctypes.c_uint64),
        ("file_size", ctypes.c_uint64),
        ("block_size", ctypes.c_uint32),
        ("filename", ctypes.c_char * FTP_MAX_FILENAME),
        ("login", ctypes.c_char * FTP_MAX_LOGIN),
        ("password", ctypes.c_char * FTP_MAX_PASSWORD),
    ]

request = Request()
request.version = 1
request.type = 1
request.block_size = 4096
request.filename = b"demo_q14_reconnect.bin"

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 3001))
sock.sendall(bytes(request))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
sock.close()
PY
printf "get %s\nbye\n" "$RECONNECT_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" 2>&1 \
  | tee "$CLIENT_Q14_LOG" \
  | prefix_live_output "client-q10-crash" "$COLOR_CLIENT"

SERVER_HASH="$(sha256sum "$SERVER_RECONNECT_PATH" | cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum "$CLIENT_RECONNECT_PATH" | cut -d ' ' -f1)"
[[ "$SERVER_HASH" == "$CLIENT_HASH" ]]

echo "==========================================="
echo "[7/12] Demonstrate Q13 client redirection through master"
rm -f "$CLIENT_TEXT_PATH"
printf "get %s\nbye\n" "$TEXT_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" 2>&1 \
  | tee "$CLIENT_Q13_LOG" \
  | prefix_live_output "client-q13" "$COLOR_CLIENT"

grep -q "Redirection vers le slave" "$CLIENT_Q13_LOG"
grep -q "redirected client" "$MASTER_LOG"
test -f "$CLIENT_TEXT_PATH"

echo "==========================================="
echo "[8/12] Demonstrate Q14 client reconnection after worker crash"
rm -f "$CLIENT_RECONNECT_PATH"
printf "get %s\nbye\n" "$RECONNECT_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" 2>&1 \
  | tee "$CLIENT_Q14_LOG" \
  | prefix_live_output "client-q14" "$COLOR_CLIENT" &
PID_Q14=$!

wait_for_log "$CLIENT_Q14_LOG" "Redirection vers le slave" 2
REDIRECTED_SLAVE=$(grep -m1 "Redirection vers le slave" "$CLIENT_Q14_LOG" | grep -oE 'slave [0-9]+' | cut -d' ' -f2)
echo "    Redirected first to slave $REDIRECTED_SLAVE"
if [[ "$REDIRECTED_SLAVE" -eq 1 ]]; then
  REDIRECTED_PARENT_PID="${PIDS[0]}"
else
  REDIRECTED_PARENT_PID="${PIDS[1]}"
fi
sleep 0.5
REDIRECTED_WORKER_PID="$(pgrep -P "$REDIRECTED_PARENT_PID" | head -n 1 || true)"
if [[ -n "$REDIRECTED_WORKER_PID" ]]; then
  echo "    Killing worker PID $REDIRECTED_WORKER_PID to force reconnection"
  kill -KILL "$REDIRECTED_WORKER_PID" 2>/dev/null || true
fi
wait "$PID_Q14" 2>/dev/null || true

SERVER_HASH="$(sha256sum "$SERVER_RECONNECT_PATH" | cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum "$CLIENT_RECONNECT_PATH" | cut -d ' ' -f1)"
[[ "$SERVER_HASH" == "$CLIENT_HASH" ]]

echo "==========================================="
echo "[9/12] Restart clean cluster for Q15-Q17"
stop_cluster
start_cluster

echo "==========================================="
echo "[10/12] Demonstrate Q15-Q17: ls, auth and put through master"
printf "ls\nput %s\nauth admin wrong\nauth admin srftp\nput %s\nls\nbye\n" "$UPLOAD_FILE" "$UPLOAD_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" 2>&1 \
  | tee "$CLIENT_Q15_Q17_LOG" \
  | prefix_live_output "client-q15-q17" "$COLOR_CLIENT"

grep -q "$LIST_SEED_FILE" "$CLIENT_Q15_Q17_LOG"
grep -q "AUTH_REQUIRED" "$CLIENT_Q15_Q17_LOG"
grep -q "AUTH_FAILED" "$CLIENT_Q15_Q17_LOG"
grep -q "Authentication successful." "$CLIENT_Q15_Q17_LOG"
grep -q "Upload successfully complete." "$CLIENT_Q15_Q17_LOG"
grep -q "$UPLOAD_FILE" "$CLIENT_Q15_Q17_LOG"
test -f "$SERVER_UPLOAD_PATH"
PUT_SOURCE_SLAVE="$(grep -m1 "Redirection vers le slave" "$CLIENT_Q15_Q17_LOG" | grep -oE 'slave [0-9]+' | cut -d' ' -f2)"
if [[ "$PUT_SOURCE_SLAVE" -eq 1 ]]; then
  grep -q "applied replicated PUT '$UPLOAD_FILE'" "$SLAVE2_LOG"
else
  grep -q "applied replicated PUT '$UPLOAD_FILE'" "$SLAVE1_LOG"
fi

echo "==========================================="
echo "[11/12] Demonstrate Q16 removal after authentication"
printf "ls\nauth admin srftp\nrm %s\nls\nbye\n" "$UPLOAD_FILE" \
  | "$ROOT_DIR/bin/clientFTP" "$HOST" 2>&1 \
  | tee "$CLIENT_Q16_RM_LOG" \
  | prefix_live_output "client-q16-rm" "$COLOR_CLIENT"

grep -q "$UPLOAD_FILE" "$CLIENT_Q16_RM_LOG"
grep -q "File '$UPLOAD_FILE' removed." "$CLIENT_Q16_RM_LOG"
test ! -f "$SERVER_UPLOAD_PATH"
RM_SOURCE_SLAVE="$(grep -m1 "Redirection vers le slave" "$CLIENT_Q16_RM_LOG" | grep -oE 'slave [0-9]+' | cut -d' ' -f2)"
if [[ "$RM_SOURCE_SLAVE" -eq 1 ]]; then
  grep -q "applied replicated RM '$UPLOAD_FILE'" "$SLAVE2_LOG"
else
  grep -q "applied replicated RM '$UPLOAD_FILE'" "$SLAVE1_LOG"
fi

echo "==========================================="
echo "[12/12] Summary"
echo "- Q1-Q10: GET, multi-command sessions, resume and abrupt client disconnect handling still work"
echo "- Q11-Q12: master registers the slaves before listening"
echo "- Q13: client is redirected automatically by the master"
echo "- Q14: client reconnects and resumes after slave failure"
echo "- Q15: LS lists server files"
echo "- Q16: PUT uploads and RM removes files, then propagates them to the other slave"
echo "- Q17: PUT/RM are protected by AUTH"
echo "PASS"
