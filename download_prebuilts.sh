#!/bin/bash
#
# download_prebuilts.sh — 恢复开源精简版中被剔除的第三方预编译产物
#
# 开源版仓库只保留源码与交叉工具链；第三方预编译库、厂商工具包、
# 官方 PDF、字体、离线浏览器包等已被剔除。本脚本按清单校验并恢复它们。
#
# 用法:
#   ./download_prebuilts.sh --list                 列出清单中全部文件
#   ./download_prebuilts.sh --check                检查当前工作区缺哪些（不下载）
#   ./download_prebuilts.sh --restore              从归档源下载恢复
#   ./download_prebuilts.sh --restore --verify     下载后校验 sha256（推荐）
#   ./download_prebuilts.sh --restore --from DIR   从一份完整 SDK 本地目录复制
#   ./download_prebuilts.sh --verify               仅校验已存在的文件
#
# 归档源（二选一）:
#   RK_PREBUILTS_URL=https://your-mirror/sdk-prebuilds ./download_prebuilts.sh --restore --verify
#   ./download_prebuilts.sh --restore --url-template 'https://host/dl?p={path}'
#
# 清单格式（TSV，三列）: sha256 <TAB> size <TAB> path
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 本脚本与清单文件都放在仓库根目录；如需指定别处，设置 REPO_ROOT 环境变量
REPO_ROOT="${REPO_ROOT:-$SCRIPT_DIR}"
MANIFEST="$SCRIPT_DIR/opensource-dropped-files.tsv"

ACTION=""
VERIFY=0
JOBS=4
FROM_DIR=""
URL_TEMPLATE="${RK_PREBUILTS_URL:-}"

usage() { sed -n '3,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0; }

while [ $# -gt 0 ]; do
  case "$1" in
    --list)          ACTION="list" ;;
    --check)         ACTION="check" ;;
    --restore)       ACTION="restore" ;;
    --verify)        VERIFY=1; [ -z "$ACTION" ] && ACTION="verify" ;;
    --from)          FROM_DIR="${2:-}"; shift ;;
    --url-template)  URL_TEMPLATE="${2:-}"; shift ;;
    --manifest)      MANIFEST="${2:-}"; shift ;;
    --jobs|-j)       JOBS="${2:-4}"; shift ;;
    -h|--help)       usage ;;
    *) echo "未知参数: $1（用 --help 查看用法）" >&2; exit 2 ;;
  esac
  shift
done

[ -n "$ACTION" ] || usage
[ -f "$MANIFEST" ] || { echo "找不到清单文件: $MANIFEST" >&2; exit 1; }

# 选择 sha256 工具
if command -v sha256sum >/dev/null 2>&1; then
  SHA_CMD() { sha256sum "$1" | awk '{print $1}'; }
elif command -v shasum >/dev/null 2>&1; then
  SHA_CMD() { shasum -a 256 "$1" | awk '{print $1}'; }
else
  echo "警告: 未找到 sha256sum/shasum，无法校验" >&2
  SHA_CMD() { echo "NOHASH"; }
fi

human() { awk -v b="$1" 'BEGIN{s=b+0; if(s>=1073741824) printf "%.2f GiB", s/1073741824; else if(s>=1048576) printf "%.1f MiB", s/1048576; else if(s>=1024) printf "%.1f KiB", s/1024; else printf "%d B", s}'; }

total_n=0; total_s=0
have_n=0;  have_s=0
bad_n=0

case "$ACTION" in

  list)
    printf '%-12s %-12s %s\n' "SHA256(前12位)" "体积" "路径"
    while IFS=$'\t' read -r H S P; do
      [ -z "${P:-}" ] && continue
      printf '%-12s %-12s %s\n' "${H:0:12}" "$(human "$S")" "$P"
    done < "$MANIFEST"
    ;;

  check|verify)
    while IFS=$'\t' read -r H S P; do
      [ -z "${P:-}" ] && continue
      total_n=$((total_n+1)); total_s=$((total_s+S))
      f="$REPO_ROOT/$P"
      if [ -f "$f" ]; then
        have_n=$((have_n+1)); have_s=$((have_s+S))
        if [ "$ACTION" = "verify" ] && [ "$VERIFY" = "1" ]; then
          g="$(SHA_CMD "$f")"
          if [ "$g" != "$H" ]; then
            echo "校验失败: $P"
            echo "  期望 $H"
            echo "  实际 $g"
            bad_n=$((bad_n+1))
          fi
        fi
      else
        if [ "$ACTION" = "check" ]; then echo "缺失: $P"; fi
      fi
    done < "$MANIFEST"

    echo
    if [ "$ACTION" = "check" ]; then
      echo "清单合计 : $total_n 个文件, $(human "$total_s")"
      echo "工作区已有: $have_n 个文件, $(human "$have_s")"
      echo "仍缺失   : $((total_n-have_n)) 个文件, $(human "$((total_s-have_s))")"
    else
      echo "已校验 $have_n/$total_n 个文件，校验失败 $bad_n 个"
    fi
    [ "$bad_n" -gt 0 ] && exit 1
    ;;

  restore)
    if [ -n "$FROM_DIR" ]; then
      [ -d "$FROM_DIR" ] || { echo "源目录不存在: $FROM_DIR" >&2; exit 1; }
      echo "从本地目录恢复: $FROM_DIR"
    elif [ -n "$URL_TEMPLATE" ]; then
      echo "从归档源恢复: $URL_TEMPLATE"
    else
      echo "未指定来源。请设置 RK_PREBUILTS_URL 或使用 --from DIR / --url-template。" >&2
      echo "提示: 先跑 --list 看看需要哪些文件，再决定从哪儿取。" >&2
      exit 1
    fi

    ok=0; skip=0; fail=0
    while IFS=$'\t' read -r H S P; do
      [ -z "${P:-}" ] && continue
      dst="$REPO_ROOT/$P"

      # 已存在且校验通过 -> 跳过
      if [ -f "$dst" ]; then
        if [ "$VERIFY" = "1" ]; then
          g="$(SHA_CMD "$dst")"
          if [ "$g" = "$H" ]; then skip=$((skip+1)); continue; fi
          echo "内容不符，重新获取: $P"
        else
          skip=$((skip+1)); continue
        fi
      fi

      mkdir -p "$(dirname "$dst")"

      if [ -n "$FROM_DIR" ]; then
        if [ -f "$FROM_DIR/$P" ]; then
          cp -f "$FROM_DIR/$P" "$dst" || { echo "复制失败: $P"; fail=$((fail+1)); continue; }
        else
          echo "源目录中不存在: $P"; fail=$((fail+1)); continue
        fi
      else
        url="$URL_TEMPLATE"
        case "$url" in
          *"{"*) url="$(printf '%s' "$url" | sed "s|{path}|$P|g")" ;;
          *)     url="${url%/}/$P" ;;
        esac
        if ! curl -fL --retry 3 --retry-delay 2 --connect-timeout 15 -o "$dst.part" "$url" 2>/dev/null; then
          echo "下载失败: $url"
          rm -f "$dst.part"; fail=$((fail+1)); continue
        fi
        mv -f "$dst.part" "$dst"
      fi

      if [ "$VERIFY" = "1" ]; then
        g="$(SHA_CMD "$dst")"
        if [ "$g" != "$H" ]; then
          echo "校验失败: $P"
          echo "  期望 $H"
          echo "  实际 $g"
          fail=$((fail+1)); continue
        fi
      fi
      ok=$((ok+1))
      printf '已恢复 %s\n' "$P"
    done < "$MANIFEST"

    echo
    echo "完成: 新恢复 $ok 个, 已存在跳过 $skip 个, 失败 $fail 个"
    [ "$fail" -gt 0 ] && exit 1
    ;;

esac

exit 0
