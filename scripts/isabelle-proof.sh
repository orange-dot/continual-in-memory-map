#!/usr/bin/env bash
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ISABELLE_BIN=${ISABELLE:-isabelle}
ISABELLE_USER_HOME=${ISABELLE_USER_HOME:-"$ROOT/.isabelle-user"}
ISABELLE_COMMAND_TIMEOUT=${ISABELLE_COMMAND_TIMEOUT:-60}
THEORY=CIM_V0a_Log
SETTINGS="$ISABELLE_USER_HOME/.isabelle/etc/settings"
LOG_DIR="$ISABELLE_USER_HOME/proof-logs"
LOG="$LOG_DIR/cim-v0a-process.log"

mkdir -p "$(dirname "$SETTINGS")" "$LOG_DIR"
if [ ! -f "$SETTINGS" ]; then
  cp "$ROOT/docs/isabelle/settings" "$SETTINGS"
fi
rm -f "$LOG"

setsid bash -c '
  cd "$1"
  USER_HOME="$2" "$3" process -d . -l HOL -e "use_thy \"$4\""
' sh "$ROOT/docs/isabelle" "$ISABELLE_USER_HOME" "$ISABELLE_BIN" "$THEORY" >"$LOG" 2>&1 &
pid=$!

stop_isabelle() {
  kill -TERM -- "-$pid" 2>/dev/null || true
  ps -eo pid=,ppid=,pgid=,cmd= |
    awk 'index($0, "poly ") && index($0, "use_thy \"'"$THEORY"'\"") { print $1, $2, $3 }' |
    while read -r child parent group; do
      kill -TERM -- "-$group" "$parent" "$child" 2>/dev/null || true
    done
  sleep 0.25
  kill -KILL -- "-$pid" 2>/dev/null || true
  ps -eo pid=,ppid=,pgid=,cmd= |
    awk 'index($0, "poly ") && index($0, "use_thy \"'"$THEORY"'\"") { print $1, $2, $3 }' |
    while read -r child parent group; do
      kill -KILL -- "-$group" "$parent" "$child" 2>/dev/null || true
    done
}

trap stop_isabelle INT TERM EXIT

deadline=$((SECONDS + ISABELLE_COMMAND_TIMEOUT))
while kill -0 "$pid" 2>/dev/null; do
  if grep -E '### theory "([^"]+\.)?'"$THEORY"'"' "$LOG" >/dev/null 2>&1; then
    stop_isabelle
    wait "$pid" 2>/dev/null || true
    if grep -E '^\*\*\*|^Exception-|FAILED|Timeout' "$LOG" >/dev/null 2>&1; then
      sed -n '1,240p' "$LOG"
      exit 1
    fi
    sed -n '1,240p' "$LOG"
    printf 'isabelle proof: loaded %s\n' "$THEORY"
    exit 0
  fi

  if grep -E '^\*\*\*|^Exception-|FAILED|Timeout' "$LOG" >/dev/null 2>&1; then
    stop_isabelle
    wait "$pid" 2>/dev/null || true
    sed -n '1,240p' "$LOG"
    exit 1
  fi

  if [ "$SECONDS" -ge "$deadline" ]; then
    stop_isabelle
    wait "$pid" 2>/dev/null || true
    sed -n '1,240p' "$LOG"
    printf 'isabelle proof: timed out after %ss\n' "$ISABELLE_COMMAND_TIMEOUT" >&2
    exit 124
  fi

  sleep 0.25
done

set +e
wait "$pid"
code=$?
set -e
sed -n '1,240p' "$LOG"
if [ "$code" -eq 0 ]; then
  printf 'isabelle proof: loaded %s\n' "$THEORY"
fi
exit "$code"
