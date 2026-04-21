#!/usr/bin/env bash

set -u

HOST="127.0.0.1"
PORT="1502"
BYTE_ORDER="big"

CMD_REF=1
STATE_REF=2
POSITION_REF=3
ATTITUDE_REF=9
CONFIDENCE_REF=15

POSITION_WORDS=6
ATTITUDE_WORDS=6
CONFIDENCE_WORDS=2

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "缺少命令: $1"
    exit 1
  fi
}

pause_wait() {
  printf "\n按回车继续..."
  read -r _
}

print_config() {
  cat <<EOF

当前配置:
  HOST        : ${HOST}
  PORT        : ${PORT}
  BYTE_ORDER  : ${BYTE_ORDER}

寄存器(按 modpoll 1-based 地址):
  cmd_reg     : ${CMD_REF}
  state_reg   : ${STATE_REF}
  position    : ${POSITION_REF} ~ $((POSITION_REF + POSITION_WORDS - 1))
  attitude    : ${ATTITUDE_REF} ~ $((ATTITUDE_REF + ATTITUDE_WORDS - 1))
  confidence  : ${CONFIDENCE_REF} ~ $((CONFIDENCE_REF + CONFIDENCE_WORDS - 1))
EOF
}

run_cmd() {
  echo
  echo "+ $*"
  "$@"
}

float_flag() {
  if [[ "${BYTE_ORDER}" == "big" ]]; then
    printf -- "-f"
  else
    printf -- ""
  fi
}

read_state() {
  run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4 -r "${STATE_REF}" -c 1 "${HOST}"
}

read_position_words() {
  run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4 -r "${POSITION_REF}" -c "${POSITION_WORDS}" "${HOST}"
}

read_attitude_words() {
  run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4 -r "${ATTITUDE_REF}" -c "${ATTITUDE_WORDS}" "${HOST}"
}

read_confidence_words() {
  run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4 -r "${CONFIDENCE_REF}" -c "${CONFIDENCE_WORDS}" "${HOST}"
}

read_position_float() {
  local maybe_f
  maybe_f="$(float_flag)"
  if [[ -n "${maybe_f}" ]]; then
    run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4:f32 -r "${POSITION_REF}" -c 3 "${maybe_f}" "${HOST}"
  else
    run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4:f32 -r "${POSITION_REF}" -c 3 "${HOST}"
  fi
}

read_attitude_float() {
  local maybe_f
  maybe_f="$(float_flag)"
  if [[ -n "${maybe_f}" ]]; then
    run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4:f32 -r "${ATTITUDE_REF}" -c 3 "${maybe_f}" "${HOST}"
  else
    run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4:f32 -r "${ATTITUDE_REF}" -c 3 "${HOST}"
  fi
}

read_confidence_float() {
  local maybe_f
  maybe_f="$(float_flag)"
  if [[ -n "${maybe_f}" ]]; then
    run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4:f32 -r "${CONFIDENCE_REF}" -c 1 "${maybe_f}" "${HOST}"
  else
    run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4:f32 -r "${CONFIDENCE_REF}" -c 1 "${HOST}"
  fi
}

write_start_command() {
  run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4 -r "${CMD_REF}" "${HOST}" 1
}

write_reset_command() {
  run_cmd modpoll -m tcp -p "${PORT}" -1 -t 4 -r "${CMD_REF}" "${HOST}" 0
}

full_flow() {
  echo
  echo "=== 1. 读取初始状态 ==="
  read_state
  read_position_words
  read_attitude_words
  read_confidence_words

  echo
  echo "=== 2. 写启动命令 ==="
  write_start_command

  echo
  echo "=== 3. 启动后再次读取，应看到 state=1 且数据区为 0 ==="
  read_state
  read_position_words
  read_attitude_words
  read_confidence_words

  echo
  echo "=== 4. 等检测模块服务成功并发布 JSON 后，手工回来继续 ==="
  pause_wait

  echo
  echo "=== 5. 再次读取，应看到 state=2 且位置/姿态/置信度有值 ==="
  read_state
  read_position_float
  read_attitude_float
  read_confidence_float

  echo
  echo "=== 6. 主站写复位命令 0，验证 2 -> 0 时清空全部数据 ==="
  write_reset_command
  read_state
  read_position_words
  read_attitude_words
  read_confidence_words

  echo
  echo "=== 7. 再次写启动命令 1，验证新的上升沿重新触发 ==="
  write_start_command
  read_state
}

edit_config() {
  local input=""

  printf "输入 HOST [%s]: " "${HOST}"
  read -r input
  if [[ -n "${input}" ]]; then
    HOST="${input}"
  fi

  printf "输入 PORT [%s]: " "${PORT}"
  read -r input
  if [[ -n "${input}" ]]; then
    PORT="${input}"
  fi

  printf "输入 BYTE_ORDER (big/little) [%s]: " "${BYTE_ORDER}"
  read -r input
  if [[ -n "${input}" ]]; then
    if [[ "${input}" == "big" || "${input}" == "little" ]]; then
      BYTE_ORDER="${input}"
    else
      echo "非法 BYTE_ORDER，保留原值: ${BYTE_ORDER}"
    fi
  fi
}

show_menu() {
  cat <<'EOF'

==============================
 robot_plc_crontorl modpoll 菜单
==============================
1. 读取 state_reg
2. 读取 position 原始寄存器
3. 读取 attitude 原始寄存器
4. 读取 confidence 原始寄存器
5. 读取 position 浮点值
6. 读取 attitude 浮点值
7. 读取 confidence 浮点值
8. 写启动命令 cmd_reg=1
9. 写复位命令 cmd_reg=0
0. 执行完整测试流程
C. 修改连接参数
S. 显示当前配置
Q. 退出
EOF
}

main() {
  require_command modpoll

  while true; do
    show_menu
    printf "\n请选择功能: "
    read -r choice

    case "${choice}" in
      1) read_state; pause_wait ;;
      2) read_position_words; pause_wait ;;
      3) read_attitude_words; pause_wait ;;
      4) read_confidence_words; pause_wait ;;
      5) read_position_float; pause_wait ;;
      6) read_attitude_float; pause_wait ;;
      7) read_confidence_float; pause_wait ;;
      8) write_start_command; pause_wait ;;
      9) write_reset_command; pause_wait ;;
      0) full_flow; pause_wait ;;
      C|c) edit_config ;;
      S|s) print_config; pause_wait ;;
      Q|q) exit 0 ;;
      *) echo "无效选择: ${choice}"; pause_wait ;;
    esac
  done
}

main "$@"
