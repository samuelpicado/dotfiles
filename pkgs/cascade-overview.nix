{ pkgs, lib }:

pkgs.gcc14Stdenv.mkDerivation {
  pname = "cascade-overview";
  version = "1.0.0";

  src = ./cascade-overview;

  nativeBuildInputs = [ pkgs.pkg-config ] ++ pkgs.hyprland.nativeBuildInputs;

  buildInputs = [ pkgs.hyprland ] ++ pkgs.hyprland.buildInputs ++ [ pkgs.lua ];

  dontUseCmakeConfigure = true;
  enableParallelBuilding = true;

  doCheck = true;
  checkPhase = ''
    runHook preCheck
    make test
    make qa-gesture-driver
    runHook postCheck
  '';

  installPhase = ''
    install -Dm755 Hyprspace.so "$out/lib/libHyprspace.so"
    install -Dm755 GestureDriver.so "$out/share/cascade-overview/tests/gesture-driver.so"
  '';

  meta = {
    description = "Cascade workspace groups and overview for Hyprland";
    homepage = "https://github.com/KZDKM/Hyprspace";
    license = lib.licenses.gpl2Only;
    platforms = lib.platforms.linux;
  };
}
