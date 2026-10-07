#!/bin/bash
# First-run consent: open a small floating terminal that explains the changes and
# asks yes/no. setup.sh makes no changes unless the user answers yes.
exec omarchy-launch-floating-terminal-with-presentation "$(dirname "$0")/setup.sh"
