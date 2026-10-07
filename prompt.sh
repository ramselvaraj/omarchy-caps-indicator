#!/bin/bash
# Asks for consent via a clickable notification; nothing is changed until accepted.
choice=$(notify-send -a "Caps Lock Indicator" -A accept="Set it up" -A later="Not now" \
  "Caps Lock Indicator needs one-time setup" \
  "It replaces fcitx5 as the input method and sets QT_IM_MODULE=wayland for Qt apps. Undo any time with uninstall.sh.")
[[ $choice == accept ]] && "$(dirname "$0")/setup.sh"
exit 0
