#!/bin/bash
#
# download_prebuilts.sh - restore the third-party prebuilt artifacts that were removed from
#                         the source-only edition of this SDK.
#
# The public repository keeps sources and the cross toolchains only. Third-party prebuilt
# libraries, vendor tool packages, official PDFs, fonts and offline browser packages have
# been removed. This script reports and restores them from a manifest.
#
# Usage:
#   ./download_prebuilts.sh --list                 list every file in the manifest
#   ./download_prebuilts.sh --check                report which files the tree lacks (no download)
#   ./download_prebuilts.sh --restore              download and restore from an archive source
#   ./download_prebuilts.sh --restore --verify     download, then verify sha256 (recommended)
#   ./download_prebuilts.sh --restore --from DIR   copy from a local copy of the full SDK
#   ./download_prebuilts.sh --verify               verify files that are already present
#
# Archive source (pick one):
#   RK_PREBUILTS_URL=https://your-mirror/sdk-prebuilds ./download_prebuilts.sh --restore --verify
#   ./download_prebuilts.sh --restore --url-template 'https://host/dl?p={path}'
#
# Manifest format (TSV, three columns): sha256 <TAB> size <TAB> path
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# This script and the manifest live in the repository root.
# Set REPO_ROOT to point the script at a different working tree.
REPO_ROOT="${REPO_ROOT:-$SCRIPT_DIR}"
MANIFEST="$SCRIPT_DIR/opensource-dropped-files.tsv"

ACTION=""
VERIFY=0
JOBS=4
FROM_DIR=""
URL_TEMPLATE="${RK_PREBUILTS_URL:-}"

# Print the leading comment block above (lines 3.. up to the first non-comment line).
usage() { awk 'NR>=3 && /^#/ {sub(/^# ?/, ""); print; next} NR>=3 {exit}' "$0"; exit 0; }

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
    *) echo "Unknown option: $1 (run --help for usage)" >&2; exit 2 ;;
  esac
  shift
done

[ -n "$ACTION" ] || usage
[ -f "$MANIFEST" ] || { echo "Manifest not found: $MANIFEST" >&2; exit 1; }

# Pick a sha256 implementation.
if command -v sha256sum >/dev/null 2>&1; then
  SHA_CMD() { sha256sum "$1" | awk '{print $1}'; }
elif command -v shasum >/dev/null 2>&1; then
  SHA_CMD() { shasum -a 256 "$1" | awk '{print $1}'; }
else
  echo "Warning: neither sha256sum nor shasum found; verification disabled" >&2
  SHA_CMD() { echo "NOHASH"; }
fi

human() { awk -v b="$1" 'BEGIN{s=b+0; if(s>=1073741824) printf "%.2f GiB", s/1073741824; else if(s>=1048576) printf "%.1f MiB", s/1048576; else if(s>=1024) printf "%.1f KiB", s/1024; else printf "%d B", s}'; }

total_n=0; total_s=0
have_n=0;  have_s=0
bad_n=0

case "$ACTION" in

  list)
    printf '%-12s %-12s %s\n' "SHA256(first12)" "SIZE" "PATH"
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
            echo "Checksum mismatch: $P"
            echo "  expected $H"
            echo "  actual   $g"
            bad_n=$((bad_n+1))
          fi
        fi
      else
        if [ "$ACTION" = "check" ]; then echo "Missing: $P"; fi
      fi
    done < "$MANIFEST"

    echo
    if [ "$ACTION" = "check" ]; then
      echo "Manifest total  : $total_n files, $(human "$total_s")"
      echo "Present in tree : $have_n files, $(human "$have_s")"
      echo "Still missing   : $((total_n-have_n)) files, $(human "$((total_s-have_s))")"
    else
      echo "Verified $have_n/$total_n files, $bad_n checksum failure(s)"
    fi
    [ "$bad_n" -gt 0 ] && exit 1
    ;;

  restore)
    if [ -n "$FROM_DIR" ]; then
      [ -d "$FROM_DIR" ] || { echo "Source directory does not exist: $FROM_DIR" >&2; exit 1; }
      echo "Restoring from local directory: $FROM_DIR"
    elif [ -n "$URL_TEMPLATE" ]; then
      echo "Restoring from archive source: $URL_TEMPLATE"
    else
      echo "No source given. Set RK_PREBUILTS_URL, or use --from DIR / --url-template." >&2
      echo "Tip: run --list first to see which files are needed." >&2
      exit 1
    fi

    ok=0; skip=0; fail=0
    while IFS=$'\t' read -r H S P; do
      [ -z "${P:-}" ] && continue
      dst="$REPO_ROOT/$P"

      # Already present and verified -> skip.
      if [ -f "$dst" ]; then
        if [ "$VERIFY" = "1" ]; then
          g="$(SHA_CMD "$dst")"
          if [ "$g" = "$H" ]; then skip=$((skip+1)); continue; fi
          echo "Content differs, fetching again: $P"
        else
          skip=$((skip+1)); continue
        fi
      fi

      mkdir -p "$(dirname "$dst")"

      if [ -n "$FROM_DIR" ]; then
        if [ -f "$FROM_DIR/$P" ]; then
          cp -f "$FROM_DIR/$P" "$dst" || { echo "Copy failed: $P"; fail=$((fail+1)); continue; }
        else
          echo "Not present in source directory: $P"; fail=$((fail+1)); continue
        fi
      else
        url="$URL_TEMPLATE"
        case "$url" in
          *"{"*) url="$(printf '%s' "$url" | sed "s|{path}|$P|g")" ;;
          *)     url="${url%/}/$P" ;;
        esac
        if ! curl -fL --retry 3 --retry-delay 2 --connect-timeout 15 -o "$dst.part" "$url" 2>/dev/null; then
          echo "Download failed: $url"
          rm -f "$dst.part"; fail=$((fail+1)); continue
        fi
        mv -f "$dst.part" "$dst"
      fi

      if [ "$VERIFY" = "1" ]; then
        g="$(SHA_CMD "$dst")"
        if [ "$g" != "$H" ]; then
          echo "Checksum mismatch: $P"
          echo "  expected $H"
          echo "  actual   $g"
          fail=$((fail+1)); continue
        fi
      fi
      ok=$((ok+1))
      printf 'Restored %s\n' "$P"
    done < "$MANIFEST"

    echo
    echo "Done: $ok restored, $skip already present, $fail failed"
    [ "$fail" -gt 0 ] && exit 1
    ;;

esac

exit 0
