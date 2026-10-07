#!/bin/bash
# One-time setup. Qt apps default to the fcitx plugin in Omarchy; point them at
# the compositor instead so they report their caret. Re-login afterwards.
# Undo: rm ~/.config/environment.d/99-capsim.conf
set -euo pipefail
mkdir -p "$HOME/.config/environment.d"
printf 'QT_IM_MODULE=wayland\nSDL_IM_MODULE=\n' > "$HOME/.config/environment.d/99-capsim.conf"
systemctl --user disable --now omarchy-fcitx5.service 2>/dev/null || true
echo "done: log out and back in so Qt apps pick up QT_IM_MODULE=wayland"
