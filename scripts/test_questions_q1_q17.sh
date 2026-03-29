#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
HOST="127.0.0.1"
SLAVE1_ID=1
SLAVE2_ID=2
SLAVE1_PORT=3001
LOG_DIR="$ROOT_DIR/logs"

SLAVE1_LOG="$LOG_DIR/test_q_slave1.log"
SLAVE2_LOG="$LOG_DIR/test_q_slave2.log"
MASTER_LOG="$LOG_DIR/test_q_master.log"

TEXT_FILE="qtest_text.txt"
TEXT_FILE_2="qtest_text_2.txt"
BIN_FILE="qtest_block.bin"
RESUME_FILE="qtest_resume.bin"
RECONNECT_FILE="qtest_reconnect.bin"
UPLOAD_FILE="qtest_upload.txt"
LIST_SEED_FILE="qtest_list_seed.txt"
MISSING_FILE="qtest_missing.txt"

SERVER_TEXT_PATH="$ROOT_DIR/data_server/$TEXT_FILE"
SERVER_TEXT_PATH_2="$ROOT_DIR/data_server/$TEXT_FILE_2"
SERVER_BIN_PATH="$ROOT_DIR/data_server/$BIN_FILE"
SERVER_RESUME_PATH="$ROOT_DIR/data_server/$RESUME_FILE"
SERVER_RECONNECT_PATH="$ROOT_DIR/data_server/$RECONNECT_FILE"
SERVER_UPLOAD_PATH="$ROOT_DIR/data_server/$UPLOAD_FILE"
SERVER_LIST_SEED_PATH="$ROOT_DIR/data_server/$LIST_SEED_FILE"

CLIENT_TEXT_PATH="$ROOT_DIR/data_client/$TEXT_FILE"
CLIENT_TEXT_PATH_2="$ROOT_DIR/data_client/$TEXT_FILE_2"
CLIENT_BIN_PATH="$ROOT_DIR/data_client/$BIN_FILE"
CLIENT_RESUME_PATH="$ROOT_DIR/data_client/$RESUME_FILE"
CLIENT_RECONNECT_PATH="$ROOT_DIR/data_client/$RECONNECT_FILE"
CLIENT_UPLOAD_PATH="$ROOT_DIR/data_client/$UPLOAD_FILE"

PIDS=()

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

pass() {
  echo "PASS: $*"
}

assert_file_contains() {
  local pattern="$1"
  local file="$2"
  grep -q "$pattern" "$file" || fail "missing pattern '$pattern' in $file"
}

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

  [[ -f "$file" ]] && cat "$file" >&2
  fail "timeout waiting for '$pattern' in $file"
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

start_cluster() {
  : > "$SLAVE1_LOG"
  : > "$SLAVE2_LOG"
  : > "$MASTER_LOG"

  stdbuf -oL -eL "$ROOT_DIR/bin/serverFTP" "$SLAVE1_ID" >"$SLAVE1_LOG" 2>&1 &
  PIDS+=("$!")
  stdbuf -oL -eL "$ROOT_DIR/bin/serverFTP" "$SLAVE2_ID" >"$SLAVE2_LOG" 2>&1 &
  PIDS+=("$!")

  wait_for_log "$SLAVE1_LOG" "waiting for master on control port"
  wait_for_log "$SLAVE2_LOG" "waiting for master on control port"

  stdbuf -oL -eL "$ROOT_DIR/bin/masterFTP" >"$MASTER_LOG" 2>&1 &
  PIDS+=("$!")

  wait_for_log "$MASTER_LOG" "slave 1 registered"
  wait_for_log "$MASTER_LOG" "slave 2 registered"
  wait_for_log "$MASTER_LOG" "listening on port 2121 with 2 registered slaves"
  wait_for_log "$SLAVE1_LOG" "listening on client port 3001"
  wait_for_log "$SLAVE2_LOG" "listening on client port 3002"
}

cleanup() {
  stop_cluster
  rm -f "$SERVER_TEXT_PATH" "$SERVER_TEXT_PATH_2" "$SERVER_BIN_PATH" \
        "$SERVER_RESUME_PATH" "$SERVER_RECONNECT_PATH" "$SERVER_UPLOAD_PATH" \
        "$SERVER_LIST_SEED_PATH"
  rm -f "$CLIENT_TEXT_PATH" "$CLIENT_TEXT_PATH_2" "$CLIENT_BIN_PATH" \
        "$CLIENT_RESUME_PATH" "$CLIENT_RECONNECT_PATH" "$CLIENT_UPLOAD_PATH"
}
trap cleanup EXIT

mkdir -p "$LOG_DIR" "$ROOT_DIR/data_server" "$ROOT_DIR/data_client"

echo "[Q1] typereq_t exists and includes the expected commands"
assert_file_contains "typedef enum" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "FTP_REQ_GET = 1" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "FTP_REQ_PUT = 2" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "FTP_REQ_LS = 3" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "FTP_REQ_RM = 4" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "FTP_REQ_BYE = 5" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "FTP_REQ_AUTH = 6" "$ROOT_DIR/include/ftp_shared.h"
pass "Q1"

echo "[Q2] request_t carries protocol metadata and payload fields"
assert_file_contains "typedef struct {" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "uint32_t version;" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "uint32_t type;" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "uint64_t offset;" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "uint64_t file_size;" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "char filename\\[FTP_MAX_FILENAME\\];" "$ROOT_DIR/include/ftp_shared.h"
pass "Q2"

echo "[Build] rebuilding before runtime tests"
make -C "$ROOT_DIR" clean >/dev/null
make -C "$ROOT_DIR" all >/dev/null

printf 'Question 5 text file\n' > "$SERVER_TEXT_PATH"
printf 'Second file for multi-request session\n' > "$SERVER_TEXT_PATH_2"
printf 'Visible in LS\n' > "$SERVER_LIST_SEED_PATH"
dd if=/dev/urandom of="$SERVER_BIN_PATH" bs=1M count=1 status=none
dd if=/dev/urandom of="$SERVER_RESUME_PATH" bs=1M count=2 status=none
dd if=/dev/urandom of="$SERVER_RECONNECT_PATH" bs=1M count=64 status=none
printf 'upload from client\n' > "$CLIENT_UPLOAD_PATH"
rm -f "$CLIENT_TEXT_PATH" "$CLIENT_TEXT_PATH_2" "$CLIENT_BIN_PATH" \
      "$CLIENT_RESUME_PATH" "$CLIENT_RECONNECT_PATH" "$SERVER_UPLOAD_PATH"

echo "[Q3] binaries build, master uses 2121 and workers start"
start_cluster
assert_file_contains "listening on port 2121" "$MASTER_LOG"
assert_file_contains "with 1 workers" "$SLAVE1_LOG"
pass "Q3"

echo "[Q4] clean shutdown propagates to the cluster"
stop_cluster
sleep 1
if pgrep -x serverFTP >/dev/null 2>&1 || pgrep -x masterFTP >/dev/null 2>&1; then
  fail "cluster processes still alive after SIGINT"
fi
pass "Q4"

start_cluster

echo "[Q5] client and server use separate working directories"
printf "get %s\nbye\n" "$TEXT_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$LOG_DIR/q5_client.log" 2>&1
[[ -f "$CLIENT_TEXT_PATH" ]] || fail "downloaded file missing from data_client"
[[ -f "$SERVER_TEXT_PATH" ]] || fail "server source file missing from data_server"
pass "Q5"

echo "[Q6] server serves GET and reports missing files"
printf "get %s\nbye\n" "$MISSING_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$LOG_DIR/q6_client_missing.log" 2>&1 || true
assert_file_contains "NOT_FOUND" "$LOG_DIR/q6_client_missing.log"
pass "Q6"

echo "[Q7] client GET path prints success statistics"
printf "get %s\nbye\n" "$TEXT_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$LOG_DIR/q7_client.log" 2>&1
assert_file_contains "Transfer successfully complete." "$LOG_DIR/q7_client.log"
assert_file_contains "bytes received in" "$LOG_DIR/q7_client.log"
pass "Q7"

echo "[Q8] binary transfer by blocks preserves integrity"
printf "get %s\nbye\n" "$BIN_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$LOG_DIR/q8_client.log" 2>&1
[[ "$(sha256sum "$SERVER_BIN_PATH" | cut -d ' ' -f1)" == "$(sha256sum "$CLIENT_BIN_PATH" | cut -d ' ' -f1)" ]] \
  || fail "binary GET hash mismatch"
pass "Q8"

echo "[Q9] multiple requests work on a single connection"
printf "get %s\nget %s\nbye\n" "$TEXT_FILE" "$TEXT_FILE_2" | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$LOG_DIR/q9_client.log" 2>&1
[[ "$(grep -c 'Transfer successfully complete.' "$LOG_DIR/q9_client.log")" -eq 2 ]] \
  || fail "expected two successful transfers in one session"
pass "Q9"

echo "[Q10] resume and client-crash robustness both work"
head -c 700000 "$SERVER_RESUME_PATH" > "$CLIENT_RESUME_PATH"
printf "get %s\nbye\n" "$RESUME_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$LOG_DIR/q10_resume.log" 2>&1
[[ "$(sha256sum "$SERVER_RESUME_PATH" | cut -d ' ' -f1)" == "$(sha256sum "$CLIENT_RESUME_PATH" | cut -d ' ' -f1)" ]] \
  || fail "resume hash mismatch"

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
request.filename = b"qtest_reconnect.bin"

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 3001))
sock.sendall(bytes(request))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
sock.close()
PY
printf "get %s\nbye\n" "$RECONNECT_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" "$SLAVE1_PORT" >"$LOG_DIR/q10_crash.log" 2>&1
[[ "$(sha256sum "$SERVER_RECONNECT_PATH" | cut -d ' ' -f1)" == "$(sha256sum "$CLIENT_RECONNECT_PATH" | cut -d ' ' -f1)" ]] \
  || fail "post-crash hash mismatch"
pass "Q10"

echo "[Q11] cluster size and slave port configuration are declared"
assert_file_contains "#define NB_SLAVES 2" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "#define FTP_SLAVE_CLIENT_BASE_PORT 3001" "$ROOT_DIR/include/ftp_shared.h"
assert_file_contains "#define FTP_SLAVE_CTRL_BASE_PORT 4001" "$ROOT_DIR/include/ftp_shared.h"
pass "Q11"

echo "[Q12] master/slave registration happens before serving clients"
assert_file_contains "slave 1 registered" "$MASTER_LOG"
assert_file_contains "slave 2 registered" "$MASTER_LOG"
assert_file_contains "registered to master" "$SLAVE1_LOG"
assert_file_contains "registered to master" "$SLAVE2_LOG"
pass "Q12"

echo "[Q13] client is redirected by the master"
printf "get %s\nbye\n" "$TEXT_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" >"$LOG_DIR/q13_client.log" 2>&1
assert_file_contains "Redirection vers le slave" "$LOG_DIR/q13_client.log"
assert_file_contains "redirected client" "$MASTER_LOG"
pass "Q13"

echo "[Q14] client reconnects after a worker crash"
rm -f "$CLIENT_RECONNECT_PATH"
printf "get %s\nbye\n" "$RECONNECT_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" >"$LOG_DIR/q14_client.log" 2>&1 &
PID_Q14=$!
wait_for_log "$LOG_DIR/q14_client.log" "Redirection vers le slave" 3
REDIRECTED_SLAVE="$(grep -m1 "Redirection vers le slave" "$LOG_DIR/q14_client.log" | grep -oE 'slave [0-9]+' | cut -d' ' -f2)"
if [[ "$REDIRECTED_SLAVE" -eq 1 ]]; then
  REDIRECTED_PARENT_PID="${PIDS[0]}"
else
  REDIRECTED_PARENT_PID="${PIDS[1]}"
fi
sleep 0.5
REDIRECTED_WORKER_PID="$(pgrep -P "$REDIRECTED_PARENT_PID" | head -n 1 || true)"
[[ -n "$REDIRECTED_WORKER_PID" ]] || fail "unable to find worker pid to kill"
kill -KILL "$REDIRECTED_WORKER_PID"
wait "$PID_Q14" 2>/dev/null || true
[[ "$(sha256sum "$SERVER_RECONNECT_PATH" | cut -d ' ' -f1)" == "$(sha256sum "$CLIENT_RECONNECT_PATH" | cut -d ' ' -f1)" ]] \
  || fail "reconnect hash mismatch"
pass "Q14"

stop_cluster
start_cluster

echo "[Q15] ls returns the server directory listing"
printf "ls\nbye\n" | "$ROOT_DIR/bin/clientFTP" "$HOST" >"$LOG_DIR/q15_client.log" 2>&1
assert_file_contains "$LIST_SEED_FILE" "$LOG_DIR/q15_client.log"
pass "Q15"

echo "[Q16] put/rm mutate the server and replicate to peers"
printf "auth admin srftp\nput %s\nbye\n" "$UPLOAD_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" >"$LOG_DIR/q16_put.log" 2>&1
[[ -f "$SERVER_UPLOAD_PATH" ]] || fail "uploaded file missing from server"
PUT_SOURCE_SLAVE="$(grep -m1 "Redirection vers le slave" "$LOG_DIR/q16_put.log" | grep -oE 'slave [0-9]+' | cut -d' ' -f2)"
if [[ "$PUT_SOURCE_SLAVE" -eq 1 ]]; then
  assert_file_contains "applied replicated PUT '$UPLOAD_FILE'" "$SLAVE2_LOG"
else
  assert_file_contains "applied replicated PUT '$UPLOAD_FILE'" "$SLAVE1_LOG"
fi

printf "auth admin srftp\nrm %s\nbye\n" "$UPLOAD_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" >"$LOG_DIR/q16_rm.log" 2>&1
[[ ! -f "$SERVER_UPLOAD_PATH" ]] || fail "uploaded file still present after RM"
RM_SOURCE_SLAVE="$(grep -m1 "Redirection vers le slave" "$LOG_DIR/q16_rm.log" | grep -oE 'slave [0-9]+' | cut -d' ' -f2)"
if [[ "$RM_SOURCE_SLAVE" -eq 1 ]]; then
  assert_file_contains "applied replicated RM '$UPLOAD_FILE'" "$SLAVE2_LOG"
else
  assert_file_contains "applied replicated RM '$UPLOAD_FILE'" "$SLAVE1_LOG"
fi
pass "Q16"

echo "[Q17] auth gates dangerous commands"
printf "put %s\nauth admin wrong\nauth admin srftp\nbye\n" "$UPLOAD_FILE" | "$ROOT_DIR/bin/clientFTP" "$HOST" >"$LOG_DIR/q17_client.log" 2>&1
assert_file_contains "AUTH_REQUIRED" "$LOG_DIR/q17_client.log"
assert_file_contains "AUTH_FAILED" "$LOG_DIR/q17_client.log"
assert_file_contains "Authentication successful." "$LOG_DIR/q17_client.log"
pass "Q17"

echo "ALL QUESTIONS PASS"
