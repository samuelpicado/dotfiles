{ mkHyprlandPlugin, fetchFromGitHub, lib }:

mkHyprlandPlugin {
  pluginName = "hyprglass";
  version = "0.7.0-0.56.1";

  src = fetchFromGitHub {
    owner = "hyprnux";
    repo = "hyprglass";
    rev = "8a4c0d3b9d880bd16676bb9026eca4a0435da7c9";
    hash = "sha256-I/1TfD8VtDE44Jk3XsAYWk/Df/aTySMt0UIgs+gipx8=";
  };

  dontStrip = true;

  installPhase = ''
    mkdir -p $out/lib
    cp hyprglass.so $out/lib/
  '';

  meta = {
    description = "Hyprland plugin that adds blur, lens, diffraction, refraction effects to transparent windows";
    homepage = "https://github.com/hyprnux/hyprglass";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
  };
}
