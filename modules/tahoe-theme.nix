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
      mkdir -p $out/share/themes $out/share/backgrounds/Tahoe $out/share/gnome-background-properties
      for variant in Tahoe-Dark Tahoe-Light; do
        cp -r gtk/$variant $out/share/themes/
        mkdir -p $out/share/themes/$variant/gtk-4.0
        sassc src/targets/$variant-gtk4.scss $out/share/themes/$variant/gtk-4.0/gtk.css
      done
      cp .config/walls/Tahoe/Tahoe-5k-dark.jpg $out/share/backgrounds/Tahoe/
      cp .config/walls/Tahoe/Tahoe-5k-light.jpg $out/share/backgrounds/Tahoe/
      cat > $out/share/gnome-background-properties/tahoe.xml << 'XMLEOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE wallpapers SYSTEM "gnome-wp-list.dtd">
<wallpapers>
  <wallpaper deleted="false">
    <name>Tahoe Dark</name>
    <filename>/run/current-system/sw/share/backgrounds/Tahoe/Tahoe-5k-dark.jpg</filename>
    <options>zoom</options>
    <shade_type>dark</shade_type>
  </wallpaper>
  <wallpaper deleted="false">
    <name>Tahoe Light</name>
    <filename>/run/current-system/sw/share/backgrounds/Tahoe/Tahoe-5k-light.jpg</filename>
    <options>zoom</options>
    <shade_type>light</shade_type>
  </wallpaper>
</wallpapers>
XMLEOF
    '';
  };


in
{
  environment.systemPackages = [
    tahoe-theme
  ];
}
