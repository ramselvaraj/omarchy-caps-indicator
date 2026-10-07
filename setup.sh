#!/bin/bash
# Applies the system changes this plugin needs, after the user has consented
# (by clicking the notification or running this script). Records what it did in
# $state/accepted so uninstall.sh can reverse exactly that and nothing else.
#  - only one input method can own the seat, so disable fcitx5 (omarchy-fcitx5.service)
#  - Qt apps default to the fcitx plugin; point them at the compositor so they
#    report their caret (QT_IM_MODULE=wayland, effective after next login)
set -euo pipefail
state="${XDG_STATE_HOME:-$HOME/.local/state}/caps-indicator"
env_file="$HOME/.config/environment.d/99-capsim.conf"
[[ -f $state/accepted ]] && { echo "already set up"; exit 0; }

# Explicit consent: describe the changes and ask. Without a terminal (and
# without --yes) refuse rather than change anything silently.
if [[ ${1:-} != --yes ]]; then
  if [[ ! ( -t 0 && -t 1 ) ]]; then
    echo "setup.sh needs a terminal to ask for consent (or pass --yes)." >&2
    exit 2
  fi
  cat <<'TXT'

Caps Lock Indicator needs these one-time changes:

  1. Disable omarchy-fcitx5.service (replaces fcitx5 as your input method;
     only one input method can own the keyboard seat). The bar's
     keyboard-layout widget may stop working.
  2. Create ~/.config/environment.d/99-capsim.conf with QT_IM_MODULE=wayland
     so Qt apps report their caret. An existing file is never overwritten.

Undo anytime with uninstall.sh in the plugin folder.

TXT
  mkdir -p "$state"
  if command -v gum >/dev/null; then
    gum confirm "Apply these changes?" || { touch "$state/declined"; echo "No changes made."; exit 0; }
  else
    read -rp "Apply these changes? [y/N] " ans
    [[ $ans == [yY]* ]] || { touch "$state/declined"; echo "No changes made."; exit 0; }
  fi
fi

mkdir -p "$state" "$(dirname "$env_file")"
rm -f "$state/declined"
env_written=0
if [[ ! -e $env_file ]]; then   # never overwrite a file the user already has
  printf '# written by the caps-indicator plugin\nQT_IM_MODULE=wayland\nSDL_IM_MODULE=\n' > "$env_file"
  env_written=1
fi
fcitx_was_enabled=0
systemctl --user is-enabled --quiet omarchy-fcitx5.service 2>/dev/null && fcitx_was_enabled=1
printf 'env_written=%s\nfcitx_was_enabled=%s\n' "$env_written" "$fcitx_was_enabled" > "$state/accepted"
systemctl --user disable --now omarchy-fcitx5.service 2>/dev/null || true
echo "set up. Log out and in once so Qt apps pick up QT_IM_MODULE=wayland."
