#!/usr/bin/env bash
#
# apply.sh — Phase 12 用 Spresense build patch 適用スクリプト
#
# 目的: spresense submodule (公式 fork) を改変せず、本リポジトリで
#       Phase 12 ビルドに必要な変更を spresense 配下に "コピー / git apply"
#       で配置する。
#
# 使い方:
#   cd /path/to/GH_wk_test
#   ./tools/spresense_patches/apply.sh
#
# 効果:
#   - spresense/sdk/configs/examples/security_camera/defconfig を配置
#   - spresense/sdk/apps の readline.c / cle.c / netlib_setifstatus.c に patch 適用
#   - これらは spresense submodule 内で working tree 変更として現れるが、
#     commit はしない (公式 fork を汚さない方針)
#
# 関連: tools/spresense_patches/README.md
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SPRESENSE_DIR="${REPO_ROOT}/spresense"
APPS_DIR="${SPRESENSE_DIR}/sdk/apps"

if [ ! -d "${SPRESENSE_DIR}" ]; then
  echo "ERROR: spresense submodule not found at ${SPRESENSE_DIR}"
  echo "       Run 'git submodule update --init --recursive' first."
  exit 1
fi

if [ ! -d "${APPS_DIR}" ]; then
  echo "ERROR: spresense/sdk/apps submodule not found at ${APPS_DIR}"
  echo "       Run 'git submodule update --init --recursive' first."
  exit 1
fi

echo "==> [1/2] Copying security_camera defconfig"
DEFCONFIG_DST="${SPRESENSE_DIR}/sdk/configs/examples/security_camera/defconfig"
mkdir -p "$(dirname "${DEFCONFIG_DST}")"
cp "${SCRIPT_DIR}/security_camera_defconfig" "${DEFCONFIG_DST}"
echo "    -> ${DEFCONFIG_DST}"

echo "==> [2/2] Applying apps repo patches"
cd "${APPS_DIR}"
for patch in "${SCRIPT_DIR}"/*.patch; do
  [ -f "${patch}" ] || continue
  if git apply --reverse --check "${patch}" 2>/dev/null; then
    echo "    [SKIP] $(basename "${patch}") (already applied)"
  elif git apply --check "${patch}" 2>/dev/null; then
    git apply "${patch}"
    echo "    [OK]   $(basename "${patch}") applied"
  else
    echo "    [FAIL] $(basename "${patch}") cannot be applied cleanly"
    echo "           Check spresense submodule version compatibility"
    exit 1
  fi
done

echo
echo "✅ Patches applied. Next steps:"
echo "   cd spresense/sdk"
echo "   ./tools/config.py examples/security_camera"
echo "   make"
echo
echo "Reset (revert to upstream clean state):"
echo "   cd spresense && git checkout -- sdk/apps spresense/sdk/configs/examples/security_camera/"
