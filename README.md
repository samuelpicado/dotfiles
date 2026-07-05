# NixOS Dotfiles

Personal NixOS configuration using flakes and home-manager.

## Structure

| File | Purpose |
|------|---------|
| `flake.nix` | Entry point, defines inputs and outputs |
| `configuration.nix` | System-wide configuration |
| `home.nix` | User programs and packages |
| `hardware-configuration.nix` | Auto-generated hardware config |

## Usage

```bash
# Rebuild system
sudo nixos-rebuild switch --flake .

# Update inputs
nix flake update
```
