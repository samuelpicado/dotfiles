{ pkgs, fetchFromGitHub, lib }:

pkgs.gcc14Stdenv.mkDerivation {
  pname = "hyprspace";
  version = "0.1-unstable-2026-07-08";

  src = fetchFromGitHub {
    owner = "ImanolBarba";
    repo = "Hyprspace";
    rev = "0799be7464fac7ea959b7c6c7809dadd6c21c5aa";
    hash = "sha256-P27tvgpduDsMjk9mSti4We+a3kzYWYWznZKizvnyS+Q=";
  };

  nativeBuildInputs = [ pkgs.pkg-config ] ++ pkgs.hyprland.nativeBuildInputs;

  buildInputs = [ pkgs.hyprland ] ++ pkgs.hyprland.buildInputs ++ [ pkgs.lua ];

  dontUseCmakeConfigure = true;

  installPhase = ''
    install -Dm755 Hyprspace.so "$out/lib/libHyprspace.so"
  '';

  meta = {
    description = "Workspace overview plugin for Hyprland";
    homepage = "https://github.com/KZDKM/Hyprspace";
    license = lib.licenses.gpl2Only;
    platforms = lib.platforms.linux;
  };
}
