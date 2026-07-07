# NixOS Dotfiles

Personal NixOS configuration using flakes and home-manager.

## Structure

```
├── flake.nix              # Entry point, defines inputs and outputs
├── home.nix               # Home Manager config (shared across hosts)
├── modules/               # Shared NixOS modules
│   ├── default.nix        # Imports all modules
│   ├── audio.nix          # PipeWire
│   ├── boot.nix           # Kernel, systemd-boot, zram
│   ├── fonts.nix          # Fonts placeholder
│   ├── gnome.nix          # GNOME desktop
│   ├── networking.nix     # Timezone, NetworkManager
│   ├── nix.nix            # Nix settings, state version
│   ├── services.nix       # Docker, Flatpak, OOMD
│   ├── system-packages.nix # System-wide packages
│   └── users.nix          # User, sudo, shell
└── hosts/
    ├── x13/               # ThinkPad X13
    │   ├── configuration.nix
    │   └── hardware-configuration.nix
    └── hp830/             # HP EliteBook 830
        ├── configuration.nix
        └── hardware-configuration.nix
```

## Usage

```bash
# Rebuild system (NixOS matches hostname to configuration automatically)
sudo nixos-rebuild switch --flake .

# Rebuild specific host
sudo nixos-rebuild switch --flake .#x13
sudo nixos-rebuild switch --flake .#hp830

# Update inputs
nix flake update
```
