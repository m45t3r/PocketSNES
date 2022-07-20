{ pkgs ? import <nixpkgs> {} }:

# To use with http://od.abstraction.se/opendingux/toolchain/opendingux-gcw0-toolchain.2021-10-22.tar.xz

let
  fhs = pkgs.buildFHSUserEnv {
    name = "gcw0-toolchain";
    targetPkgs = pkgs: with pkgs; [
      git
      gnumake
      stdenv.cc.cc
    ];
  };
in fhs.env
