{ config, lib, pkgs, ... }:

let
  tahoeSrc = pkgs.fetchFromGitHub {
    owner = "kayozxo";
    repo = "GNOME-macOS-Tahoe";
    rev = "6dfcd9d941e5e76cfa58af9ab1cc835c642f56e3";
    sha256 = "sha256-N+6eR0CQsQObd22tVduvIHYfvPA69AlXTJSYne1esi4=";
  };

  tahoe-theme = pkgs.stdenvNoCC.mkDerivation {
    pname = "tahoe-theme";
    version = "unstable";
    src = tahoeSrc;
    nativeBuildInputs = [ pkgs.sassc ];
    installPhase = ''
      mkdir -p $out/share/themes $out/share/backgrounds/Tahoe
      for variant in Tahoe-Dark Tahoe-Light; do
        cp -r gtk/$variant $out/share/themes/
        mkdir -p $out/share/themes/$variant/gtk-4.0
        sassc src/targets/$variant-gtk4.scss $out/share/themes/$variant/gtk-4.0/gtk.css
      done
      cp .config/walls/Tahoe/Tahoe-5k-dark.jpg $out/share/backgrounds/Tahoe/
      cp .config/walls/Tahoe/Tahoe-5k-light.jpg $out/share/backgrounds/Tahoe/
    '';
  };

in
{
  environment.systemPackages = [
    tahoe-theme
  ];
}
