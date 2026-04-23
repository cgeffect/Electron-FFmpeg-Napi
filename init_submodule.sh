#!/usr/bin/env sh

set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
EMSDK_DIR="${ROOT_DIR}/emsdk"
EMSDK_REPO="https://github.com/emscripten-core/emsdk.git"
EMSDK_TAG="${EMSDK_TAG:-latest}"

if ! command -v git >/dev/null 2>&1; then
  echo "Error: git is required but not found in PATH."
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "Error: python3 is required but not found in PATH."
  exit 1
fi

if [ ! -d "${ROOT_DIR}/.git" ] && [ ! -f "${ROOT_DIR}/.git" ]; then
  echo "Error: ${ROOT_DIR} is not a git repository root."
  exit 1
fi

if [ ! -d "${EMSDK_DIR}/.git" ]; then
  echo "Cloning emsdk into ${EMSDK_DIR} ..."
  git -C "${ROOT_DIR}" clone "${EMSDK_REPO}" emsdk
else
  echo "Updating emsdk repository ..."
  git -C "${EMSDK_DIR}" fetch --tags --prune
fi

echo "Installing and activating emsdk (${EMSDK_TAG}) ..."
unset EMSDK_PYTHON PYTHONHOME PYTHONPATH SSL_CERT_FILE CURL_CA_BUNDLE REQUESTS_CA_BUNDLE || true
(
  cd "${EMSDK_DIR}"
  ./emsdk install "${EMSDK_TAG}"
  ./emsdk activate "${EMSDK_TAG}"
)

echo "Done. Load environment via:"
echo "  cd emsdk && source ./emsdk_env.sh"
