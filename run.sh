#!/bin/bash
# Starts capsim. Idempotent first-run setup is folded in here so the plugin is
# self-contained:
#  - only one input method can own the seat, so retire fcitx5;
#  - Qt apps default to the fcitx plugin in Omarchy; point them at the
#    compositor so they report their caret (takes effect after next login).
# Undo: see README.
env_file="$HOME/.config/environment.d/99-capsim.conf"
if [[ ! -f $env_file ]]; then
  mkdir -p "$(dirname "$env_file")"
  printf '# written by the caps-indicator plugin\nQT_IM_MODULE=wayland\nSDL_IM_MODULE=\n' > "$env_file"
  echo "wrote $env_file (log out and in once for Qt apps to pick it up)" >&2
fi
systemctl --user disable --now omarchy-fcitx5.service 2>/dev/null || systemctl --user stop omarchy-fcitx5.service 2>/dev/null || true
exec "$(dirname "$0")/bin/capsim"
