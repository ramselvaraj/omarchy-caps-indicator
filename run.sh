#!/bin/bash
# Starts capsim. Refuses to run until the user has consented (setup.sh done).
state="${XDG_STATE_HOME:-$HOME/.local/state}/caps-indicator"
[[ -f $state/accepted ]] || exit 3
systemctl --user stop omarchy-fcitx5.service 2>/dev/null || true
exec "$(dirname "$0")/bin/capsim"
