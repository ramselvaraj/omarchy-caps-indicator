#!/bin/bash
# Only one input method can own the seat, so make sure fcitx5 is not running.
systemctl --user stop omarchy-fcitx5.service 2>/dev/null || true
exec "$(dirname "$0")/bin/capsim"
