#!/usr/bin/env bash
set -euo pipefail

if [[ -f /etc/os-release ]] && grep -qi '^ID=nixos' /etc/os-release; then
  cat <<'EOF'
Detected NixOS.
Add the following to your configuration.nix (and rebuild):

  users.users.${yourUser}.extraGroups = [ "input" ];
  services.udev.extraRules = ''
    KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"
    SUBSYSTEM=="input", GROUP="input", MODE="0660"
  '';

Then: sudo nixos-rebuild switch
Log out/in to apply the input group membership.
EOF
  exit 0
fi

if [[ $EUID -ne 0 ]]; then
  echo "This script will use sudo for system changes."
fi

# Ensure user is in 'input' group
if ! id -nG "$USER" | grep -qw input; then
  sudo usermod -aG input "$USER"
  echo "Added $USER to 'input' group. You must log out/in for this to take effect."
fi

# Install udev rule to make input devices writable by 'input' group
RULE='/etc/udev/rules.d/99-ec-input.rules'
sudo bash -c "cat > '$RULE'" <<'RULES'
KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"
SUBSYSTEM=="input", GROUP="input", MODE="0660"
RULES

sudo udevadm control --reload
sudo udevadm trigger
echo "Installed $RULE and reloaded udev. Re-login may be required."
