# NixOS Dotfiles

## Overview

Personal NixOS configuration for Samuel Picado's `x13` and `hp830` hosts. Nix flakes define the systems, shared NixOS modules configure the desktop and services, and Home Manager manages the `pikdo` user environment.

## Features

- Separate host configurations for the ThinkPad X13 and HP EliteBook 830, both targeting `x86_64-linux`.
- Shared GNOME desktop configuration, extensions, and a Tahoe theme.
- PipeWire audio, NetworkManager, a CachyOS kernel, systemd-boot, and zram.
- Docker, Flatpak, Steam, and ZeroTier service configuration.
- A Home Manager environment with Zsh, Starship, fzf, zoxide, direnv, browsers, and editor settings.

## Technology

- Nix and NixOS flakes, with inputs recorded in `flake.lock`.
- [Home Manager](https://github.com/nix-community/home-manager), integrated as a NixOS module.
- [nix-cachyos-kernel](https://github.com/xddxdd/nix-cachyos-kernel) for the configured kernel packages.
- [GNOME-macOS-Tahoe](https://github.com/kayozxo/GNOME-macOS-Tahoe) as the upstream theme source packaged by `modules/tahoe-theme.nix`.

## Getting started

These configurations describe specific existing machines. To reuse them on another computer, first adapt the host and hardware configuration, filesystem devices, user account, Home Manager paths, networking, and services to that machine. Review the configured passwordless sudo policy, Docker access, firewall ports, and ZeroTier settings before applying them.

For an existing configured host, use NixOS with the `nix-command` and `flakes` features enabled, and run commands from a local checkout:

```bash
git clone https://github.com/samuelpicado/dotfiles.git
cd dotfiles
```

Choose only the output for the machine being rebuilt:

| Host | Configuration | Command |
| --- | --- | --- |
| ThinkPad X13 | `hosts/x13/` | `sudo nixos-rebuild switch --flake .#x13` |
| HP EliteBook 830 | `hosts/hp830/` | `sudo nixos-rebuild switch --flake .#hp830` |

On an already configured machine whose hostname matches one of those outputs, the shorter form from the original setup is also available:

```bash
sudo nixos-rebuild switch --flake .
```

To update the flake inputs deliberately:

```bash
nix flake update
```

This updates `flake.lock`. Review the changes and rebuild the intended host afterward. Home Manager is applied through the NixOS rebuild; this flake does not define a standalone Home Manager output.

## Repository layout

| Path | Purpose |
| --- | --- |
| `flake.nix` | Flake inputs and the `x13` and `hp830` NixOS outputs. |
| `flake.lock` | Locked flake input revisions. |
| `home.nix` | Shared Home Manager configuration for `pikdo`. |
| `hosts/x13/` | ThinkPad X13 settings and generated hardware configuration. |
| `hosts/hp830/` | HP EliteBook 830 settings and generated hardware configuration. |
| `modules/default.nix` | Imports the shared NixOS modules. |
| `modules/` | Desktop, theme, boot, audio, networking, users, packages, and services. |

## Project status

Personal system configuration with two defined hosts. Hardware settings, local paths, and service choices are specific to the owner's environment. Changes should be evaluated and validated on the intended machine before switching its system configuration.
