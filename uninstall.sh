#!/bin/bash
# Reverses setup.sh, then removes the plugin. Run this INSTEAD of a bare
# `omarchy plugin remove`, which can't undo the system changes.
set -uo pipefail
id="ramselvaraj.caps-indicator"
state="${XDG_STATE_HOME:-$HOME/.local/state}/caps-indicator"
env_file="$HOME/.config/environment.d/99-capsim.conf"

# Unload the service first so capsim is not restarted while we put fcitx5 back.
omarchy plugin disable "$id" >/dev/null 2>&1
sleep 1; pkill -x capsim 2>/dev/null

env_written=0; fcitx_was_enabled=0
[[ -f $state/accepted ]] && source "$state/accepted"
if [[ $env_written == 1 && -f $env_file ]] && grep -q 'caps-indicator plugin' "$env_file"; then
  rm -f "$env_file" && echo "removed $env_file"
fi
if [[ $fcitx_was_enabled == 1 ]]; then
  systemctl --user enable --now omarchy-fcitx5.service && echo "re-enabled fcitx5"
fi
rm -rf "$state"
echo "removing plugin (log out and in once to drop QT_IM_MODULE=wayland)"
exec omarchy plugin remove "$id" --yes
