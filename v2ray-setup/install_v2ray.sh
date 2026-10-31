#!/bin/bash
#
# install_v2ray.sh — 一键安装 V2Ray 内核 + 可用的 VMess 服务, 并提供配置管理
#
# 用法: install_v2ray.sh [命令]
#   install            安装内核 + systemd 服务 + 默认 VMess 入站, 打印 vmess 链接 (默认命令)
#   list               列出所有入站配置及对应的 vmess:// 链接
#   add [名字]          新增一个 VMess 入站 (随机端口 + 新 uuid)
#   remove <名字>       删除一个入站配置
#   version            查看内核版本
#   uninstall          卸载并删除全部配置
#
# 选项 (仅 install):
#   -f <core.zip>      使用本地内核 zip 安装
#   -v <ver>           指定内核版本, e.g. -v v5.4.1
#   -p <addr>          下载走代理, e.g. -p http://127.0.0.1:2333
#
# 安装结果:
#   /etc/v2ray/bin/v2ray        内核
#   /etc/v2ray/config.json      主配置
#   /etc/v2ray/conf/*.json      入站配置 (每个文件一个 VMess 入站)
#   /etc/v2ray/users            入站清单 (名字 端口 uuid 路径), list 命令直接读它
#   /etc/systemd/system/v2ray.service
#   /usr/local/bin/v2ray-ctl    本脚本的管理入口 (list/add/remove)

set -u

# ============================================================
# 模块: 常量
# ============================================================
CORE_NAME=V2Ray
CORE=v2ray
CORE_REPO=v2fly/v2ray-core
CORE_DIR=/etc/$CORE
CORE_BIN=$CORE_DIR/bin/$CORE
CONF_DIR=$CORE_DIR/conf
MAIN_CONF=$CORE_DIR/config.json
USERS_FILE=$CORE_DIR/users
LOG_DIR=/var/log/$CORE
SERVICE_NAME=$CORE
SERVICE_FILE=/etc/systemd/system/$SERVICE_NAME.service
CTL_BIN=/usr/local/bin/v2ray-ctl
SELF=$(readlink -f "$0")

REQUIRED_PKGS="wget unzip"

# ============================================================
# 模块: 输出与清理
# ============================================================
red='\e[31m'; yellow='\e[33m'; green='\e[92m'; none='\e[0m'

msg() {
    local color
    case $1 in warn) color=$yellow ;; err) color=$red ;; ok) color=$green ;; *) color=$none ;; esac
    echo -e "${color}$(date +'%T')${none}) $2"
}

err() { echo -e "\n\e[41m错误!\e[0m $1\n" >&2; exit 1; }
warn() { echo -e "\n\e[41m警告!\e[0m $1\n"; }

cleanup() { [[ ${TMP_DIR:-} && -d $TMP_DIR ]] && rm -rf "$TMP_DIR"; }
trap cleanup EXIT

# ============================================================
# 模块: 环境检查
# ============================================================
check_env() {
    [[ $EUID -eq 0 ]] || err "请以 ROOT 用户运行。"

    PKG_CMD=
    type -P apt-get &>/dev/null && PKG_CMD=$(type -P apt-get)
    type -P yum &>/dev/null && PKG_CMD=${PKG_CMD:-$(type -P yum)}
    [[ ${PKG_CMD:-} ]] || err "此脚本仅支持 Ubuntu / Debian / CentOS。"

    type -P systemctl &>/dev/null ||
        err "缺少 systemctl, 请先安装: $PKG_CMD install -y systemd"

    case $(uname -m) in
    x86_64 | amd64) CORE_ARCH=64 ;;
    aarch64 | armv8*) CORE_ARCH=arm64-v8a ;;
    *) err "此脚本仅支持 64 位系统。" ;;
    esac
}

# ============================================================
# 模块: 参数解析 (install 专用)
# ============================================================
show_help() { grep '^#' "$0" | tail -n +3 | sed 's/^# \{0,1\}//'; exit 0; }

parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
        -f) [[ -f ${2:-} ]] || err "($2) 不是常规文件。"; CORE_FILE=$2; shift 2 ;;
        -v) [[ -n ${2:-} ]] || err "($1) 缺少参数。"; CORE_VER="v${2#v}"; shift 2 ;;
        -p) [[ -n ${2:-} ]] || err "($1) 缺少参数。"; PROXY=$2; shift 2 ;;
        -h | --help) show_help ;;
        *) err "未知参数: $1" ;;
        esac
    done
    [[ ${CORE_VER:-} && ${CORE_FILE:-} ]] && err "无法同时指定版本和本地文件。"
}

# ============================================================
# 模块: 依赖 / 下载
# ============================================================
install_pkgs() {
    local missing=() p
    for p in $REQUIRED_PKGS; do
        type -P "$p" &>/dev/null || missing+=("$p")
    done
    ((${#missing[@]})) || return 0
    msg warn "安装依赖包: ${missing[*]}"
    "$PKG_CMD" install -y "${missing[@]}" &>/dev/null && return 0
    [[ $PKG_CMD == *yum ]] && yum install -y epel-release &>/dev/null
    "$PKG_CMD" update -y &>/dev/null
    "$PKG_CMD" install -y "${missing[@]}" &>/dev/null ||
        err "依赖包安装失败, 请手动执行: $PKG_CMD install -y ${missing[*]}"
}

_wget() {
    [[ ${PROXY:-} ]] && export https_proxy=$PROXY
    wget --no-check-certificate "$@"
}

download_core() {
    if [[ ${CORE_FILE:-} ]]; then
        msg ok "使用本地内核文件: $CORE_FILE"
        cp -f "$CORE_FILE" "$TMP_DIR/core.zip" || err "复制内核文件失败。"
        return 0
    fi
    local link
    if [[ ${CORE_VER:-} ]]; then
        link="https://github.com/${CORE_REPO}/releases/download/${CORE_VER}/${CORE}-linux-${CORE_ARCH}.zip"
    else
        link="https://github.com/${CORE_REPO}/releases/latest/download/${CORE}-linux-${CORE_ARCH}.zip"
    fi
    msg warn "下载 $CORE_NAME > $link"
    _wget -t 3 -q -c "$link" -O "$TMP_DIR/core.zip" ||
        err "下载失败, 可用 -f 指定本地文件或 -p 设置代理。"
}

get_server_ip() {
    SERVER_IP=$(_wget -4 -qO- https://one.one.one.one/cdn-cgi/trace | grep '^ip=' | cut -d= -f2)
    [[ -z $SERVER_IP ]] && SERVER_IP=$(_wget -6 -qO- https://one.one.one.one/cdn-cgi/trace | grep '^ip=' | cut -d= -f2)
    [[ -z ${SERVER_IP:-} ]] && warn "获取公网 IP 失败, vmess 链接中的地址需自行替换。"
}

# ============================================================
# 模块: VMess 入站管理
# ============================================================
gen_uuid() {
    if [[ -x $CORE_BIN ]]; then
        "$CORE_BIN" uuid
    else
        cat /proc/sys/kernel/random/uuid
    fi
}

# 随机 10000-65535 且未被占用的端口
gen_port() {
    local port
    while :; do
        port=$((RANDOM % 55536 + 10000))
        ss -tln 2>/dev/null | grep -q ":$port " || { echo "$port"; return; }
    done
}

# add_inbound <名字>  → 追加记录到 users 文件并生成 conf json
add_inbound() {
    local name=$1 port uuid
    port=$(gen_port)
    uuid=$(gen_uuid)
    cat >"$CONF_DIR/$name.json" <<EOF
{
  "inbounds": [{
    "port": $port,
    "listen": "0.0.0.0",
    "protocol": "vmess",
    "settings": {
      "clients": [{ "id": "$uuid", "alterId": 0 }]
    },
    "streamSettings": {
      "network": "ws",
      "wsSettings": { "path": "/$name" }
    }
  }]
}
EOF
    echo "$name $port $uuid /$name" >>"$USERS_FILE"
    echo "$port $uuid /$name"
}

# vmess_link <ip> <port> <uuid> <path> [备注]
vmess_link() {
    local json tag=${5:-vmess}
    json=$(printf '{"v":"2","ps":"%s","add":"%s","port":"%s","id":"%s","aid":"0","net":"ws","path":"%s","type":"none","host":"","tls":""}' \
        "$tag" "$1" "$2" "$3" "$4")
    echo "vmess://$(printf '%s' "$json" | base64 -w0)"
}

# list 所有入站并输出链接
list_inbounds() {
    [[ -f $USERS_FILE && -s $USERS_FILE ]] || { warn "尚无任何入站配置, 先执行 install 或 add。"; return 1; }
    get_server_ip
    printf '%-14s %-8s %-40s %s\n' 名字 端口 UUID 路径
    while read -r name port uuid path; do
        [[ $name ]] || continue
        printf '%-14s %-8s %-40s %s\n' "$name" "$port" "$uuid" "$path"
        echo "  $(vmess_link "${SERVER_IP:-你的服务器IP}" "$port" "$uuid" "$path" "$name")"
        echo
    done <"$USERS_FILE"
}

reload_service() {
    systemctl restart "$SERVICE_NAME" ||
        err "服务重启失败, 执行: systemctl status $SERVICE_NAME 查看。"
    systemctl is-active --quiet "$SERVICE_NAME" &&
        msg ok "服务运行中, 端口已生效。" ||
        err "服务未运行, 执行: journalctl -u $SERVICE_NAME -n 20 排查。"
}

# ============================================================
# 模块: 安装
# ============================================================
install_core() {
    msg warn "解压内核..."
    mkdir -p "$CORE_DIR/bin"
    unzip -qo "$TMP_DIR/core.zip" -d "$CORE_DIR/bin" || err "内核 zip 解压失败。"
    local f
    for f in $CORE geoip.dat geosite.dat; do
        [[ -f $CORE_DIR/bin/$f ]] || err "压缩包缺少 $f, 文件无效。"
    done
    chmod +x "$CORE_BIN"
}

write_main_config() {
    mkdir -p "$CONF_DIR" "$LOG_DIR"
    cat >"$MAIN_CONF" <<EOF
{
  "log": {
    "loglevel": "warning",
    "access": "$LOG_DIR/access.log",
    "error": "$LOG_DIR/error.log"
  },
  "inbounds": [],
  "outbounds": [
    { "protocol": "freedom" }
  ]
}
EOF
}

install_service() {
    cat >"$SERVICE_FILE" <<EOF
[Unit]
Description=$CORE_NAME Service
After=network.target nss-lookup.target

[Service]
User=root
CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_ADMIN CAP_NET_BIND_SERVICE
NoNewPrivileges=true
ExecStart=$CORE_BIN run -c $MAIN_CONF -confdir $CONF_DIR
Restart=on-failure
RestartPreventExitStatus=23

[Install]
WantedBy=multi-user.target
EOF
    systemctl daemon-reload
    systemctl enable "$SERVICE_NAME"
}

sync_time() {
    timedatectl set-ntp true &>/dev/null ||
        warn "无法设置自动同步时间, 时间偏差可能影响 VMess 协议。"
}

do_install() {
    if [[ -f $SERVICE_FILE && -x $CORE_BIN ]]; then
        warn "检测到已安装, 直接新增一个入站配置。"
        get_server_ip
        local out
        out=$(add_inbound "user$(date +%s | tail -c 5)")
        reload_service
        echo "  $(vmess_link "${SERVER_IP:-}" ${out// / })"
        return 0
    fi

    TMP_DIR=$(mktemp -d) || err "创建临时目录失败。"
    msg warn "开始安装 $CORE_NAME..."
    [[ ${CORE_VER:-} ]] && msg warn "版本: $CORE_VER"
    [[ ${PROXY:-} ]] && msg warn "代理: $PROXY"

    sync_time
    install_pkgs
    download_core
    install_core
    write_main_config
    install_service
    get_server_ip

    # 默认入站 + 管理入口
    : >"$USERS_FILE"
    local out name="vmess$(date +%s | tail -c 5)"
    out=$(add_inbound "$name")

    cp -f "$SELF" "$CTL_BIN" && chmod +x "$CTL_BIN"

    systemctl start "$SERVICE_NAME"
    reload_service

    msg ok "安装完成。"
    echo
    echo "  $(vmess_link "${SERVER_IP:-你的服务器IP}" $out "$name")"
    echo
    echo "常用命令:"
    echo "  v2ray-ctl list              查看所有配置与 vmess 链接"
    echo "  v2ray-ctl add [名字]         新增用户"
    echo "  v2ray-ctl remove <名字>      删除用户"
    echo "  systemctl status $SERVICE_NAME"
}

# ============================================================
# 模块: 入口
# ============================================================
cmd_uninstall() {
    msg warn "即将卸载: 停止并删除 v2ray 服务、/etc/v2ray、/var/log/v2ray、v2ray-ctl (所有配置将被删除)"
    systemctl stop "$SERVICE_NAME" &>/dev/null
    systemctl disable "$SERVICE_NAME" &>/dev/null
    rm -f "$SERVICE_FILE"
    systemctl daemon-reload
    rm -rf "$CORE_DIR" "$LOG_DIR" "$CTL_BIN"
    msg ok "已卸载完成, 重新执行 $0 install 可全新安装。"
}

cmd_remove() {
    [[ -n ${1:-} ]] || err "用法: $0 remove <名字>"
    local line
    line=$(grep "^$1 " "$USERS_FILE" 2>/dev/null) || err "找不到名为 $1 的配置。"
    rm -f "$CONF_DIR/$1.json"
    sed -i "/^$1 /d" "$USERS_FILE"
    reload_service
    msg ok "已删除: $line"
}

case ${1:-install} in
install)
    shift
    check_env
    parse_args "$@"
    do_install
    ;;
list)    list_inbounds ;;
add)     out=$(add_inbound "${2:-user$(date +%s | tail -c 5)}"); get_server_ip; reload_service;
         echo "  $(vmess_link "${SERVER_IP:-你的服务器IP}" $out "${2:-}")" ;;
remove)  shift; cmd_remove "${1:-}" ;;
uninstall) cmd_uninstall ;;
version) "$CORE_BIN" version ;;
-h | --help) show_help ;;
*)       err "未知命令: $1 (可用: install/list/add/remove/uninstall/version)" ;;
esac
