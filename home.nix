{ config, lib, pkgs, ... }:

{
  home.username = "pikdo";
  home.homeDirectory = "/home/pikdo";

  home.packages = with pkgs; [
    tree
    fastfetch
  ];

  programs.git = {
    enable = true;
    package = pkgs.gitFull;
    userName = "Samuel Picado";
    userEmail = "sampikdo@gmail.com";
    extraConfig = {
      init.defaultBranch = "main";
      pull.rebase = true;
    };
  };

  programs.starship.enable = true;

  programs.zsh = {
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

  home.stateVersion = "26.11";

  programs.chromium = {
    enable = true;
    extensions = [
      { id = "ddkjiahejlhfcafbddmgiahcphecmpfh"; }
      { id = "nngceckbapebfimnlniiiahkandclblb"; }
    ];
  };

  programs.direnv = {
      enable = true;
      enableBashIntegration = true;
      nix-direnv.enable = true;
    };
  programs.vscode = {
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
      userSettings = {

         };
    };
  };

}
