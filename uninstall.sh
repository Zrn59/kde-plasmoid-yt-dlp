#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/install.sh" --uninstall "$@"
