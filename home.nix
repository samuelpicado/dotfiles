{ config, lib, pkgs, osConfig, ... }:

{
  home = {
    username = "pikdo";
    homeDirectory = "/home/pikdo";
    stateVersion = "26.11";
    sessionVariables = {
      SHELL = "${pkgs.zsh}/bin/zsh";
    };
  };

  # --- Packages ---
  home.packages = with pkgs; [
    fastfetch
    opencode
    bat
    eza
    fd
    libnotify
    python3
    ripgrep
    tealdeer
    tree
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
      autocd = true;
      enableCompletion = true;
      autosuggestion.enable = true;
      syntaxHighlighting.enable = true;
      history = {
        size = 100000;
        save = 100000;
        ignoreDups = true;
        ignoreSpace = true;
        share = true;
        expireDuplicatesFirst = true;
        extended = true;
      };
      historySubstringSearch = {
        enable = true;
        searchUpKey = [ "^[[A" ];
        searchDownKey = [ "^[[B" ];
      };
      shellAliases = {
        ll = "eza -la --icons --group-directories-first";
        l = "eza -1 --icons --group-directories-first";
        ls = "l";
        lh = "eza -lah --icons --group-directories-first";
        lt = "eza -la --icons --group-directories-first --tree --level=2";
        cat = "bat --paging=never --style=plain";
        grep = "rg";
        find = "fd";
        gs = "git status";
        ga = "git add .";
        gc = "git commit -m";
        gf = "git fetch";
        gp = "git push";
        up = "nix flake update";
        rebuild = "sudo nixos-rebuild switch --flake /home/pikdo/Documents/GitHub/Dotfiles";
        hprebuild = "sudo nixos-rebuild switch --flake /home/pikdo/Dotfiles";
      };
      plugins = [
        {
          name = "you-should-use";
          src = "${pkgs.zsh-you-should-use}/share/zsh/plugins/you-should-use";
        }
        {
          name = "zsh-autocomplete";
          src = "${pkgs.zsh-autocomplete}/share/zsh-autocomplete";
        }
        {
          name = "auto-notify";
          src = pkgs.fetchFromGitHub {
            owner = "MichaelAquilina";
            repo = "zsh-auto-notify";
            rev = "b51c934d88868e56c1d55d0a2a36d559f21cb2ee";
            sha256 = "1qcigpm1n6dxa7a4rm222g92vvbmp6j6s90462bni9nfql1c2x5k";
          };
        }
      ];
      initContent = ''
        bindkey '^[[1;5C' forward-word
        bindkey '^[[1;5D' backward-word
        bindkey '^[[3~' delete-char
        bindkey '^H' backward-kill-word
        bindkey '^P' up-line-or-history
        bindkey '^N' down-line-or-history
      '';
    };

    fzf = {
      enable = true;
      enableZshIntegration = true;
      defaultCommand = "fd --type f --hidden --follow --exclude .git";
      defaultOptions = [
        "--height 40%"
        "--layout=reverse"
        "--border"
      ];
    };

    zoxide = {
      enable = true;
      enableZshIntegration = true;
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
