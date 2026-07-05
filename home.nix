{ config, lib, pkgs, ... }:

{
  home = {
    username = "pikdo";
    homeDirectory = "/home/pikdo";
    stateVersion = "26.11";
  };

  # --- Packages ---
  home.packages = with pkgs; [
    tree
    fastfetch
    opencode
  ];

  # --- Programs ---
  programs = {
    git = {
      enable = true;
      package = pkgs.gitFull;
      settings = {
        user.name = "Samuel Picado";
        user.email = "sampikdo@gmail.com";
        init.defaultBranch = "main";
        pull.rebase = true;
      };
    };

    starship.enable = true;

    zsh = {
      enable = true;
      enableCompletion = true;
      autosuggestion.enable = true;
      syntaxHighlighting.enable = true;
      shellAliases = {
        ll = "ls -la";
        up = "nix flake update";
        rebuild = "sudo nixos-rebuild switch --flake /home/pikdo/Documents/GitHub/Dotfiles";
      };
    };

    chromium = {
      enable = true;
      extensions = [
        { id = "ddkjiahejlhfcafbddmgiahcphecmpfh"; }
        { id = "nngceckbapebfimnlniiiahkandclblb"; }
      ];
    };

    direnv = {
      enable = true;
      enableBashIntegration = true;
      nix-direnv.enable = true;
    };

    vscode = {
      enable = true;
      profiles.default = {
        extensions = with pkgs.vscode-extensions; [
          esbenp.prettier-vscode
          ecmel.vscode-html-css
          humao.rest-client
          vscjava.vscode-java-pack
          james-yu.latex-workshop
          bbenoist.nix
          ms-vscode.live-server
          christian-kohler.path-intellisense
          christian-kohler.npm-intellisense
          mikestead.dotenv
          formulahendry.auto-rename-tag
          formulahendry.auto-close-tag
          shardulm94.trailing-spaces
          prisma.prisma
          pkief.material-icon-theme
          rust-lang.rust-analyzer
          tauri-apps.tauri-vscode
        ];
        userSettings = {};
      };
    };
  };
}
