# Caps Lock Indicator

![A Caps Lock pill drawn at the text cursor in a terminal](preview.png)

A macOS-style Caps Lock pill drawn right at the text caret, in any Wayland app,
filled with your Omarchy theme's accent color (it follows theme changes live).

It appears **only** when a text field has focus, the app has reported its caret
position, and Caps Lock is on. If an app doesn't report a caret, nothing is shown.

## How it works

`capsim` is a tiny Wayland `input-method-v2` client. The compositor places its
popup surface at the focused caret, and the popup holds the pill. Caps state
comes from `/sys/class/leds/*::capslock`. The service builds the binary on first
run (`build.sh`) and restarts it if it exits.

## Install

    omarchy plugin add https://github.com/ramselvaraj/omarchy-caps-indicator --enable

The plugin builds itself, then shows a notification: **"Caps Lock Indicator needs
one-time setup"**. Click **Set it up** (or run `setup.sh` from the plugin folder).
Nothing on your system is changed, and the indicator does not start, until you accept.
Log out and in once afterwards so Qt apps pick up the new input-method setting.

## What setup changes

- Disables `omarchy-fcitx5.service`. Only one input method can own the keyboard
  seat, so this **replaces fcitx5**. The bar's keyboard-layout widget depends on
  fcitx5 and may stop working.
- Creates `~/.config/environment.d/99-capsim.conf` (`QT_IM_MODULE=wayland`) so Qt
  apps talk to the compositor directly. An existing file is never overwritten.

It records exactly what it did in `~/.local/state/caps-indicator/accepted`.

## Caveats

- Apps drawing their own text fields without text-input support won't show it.

## Uninstall

    ~/.config/omarchy/plugins/ramselvaraj.caps-indicator/uninstall.sh

This re-enables fcitx5 (only if it was enabled before), deletes the env file (only
if the plugin created it), and removes the plugin. Use it instead of a bare
`omarchy plugin remove`, which cannot undo the system changes. Log out and in once
afterwards.
