{ pkgs ? import <nixpkgs> {} }:

let
  riscv = pkgs.pkgsCross.riscv64.buildPackages;
in

pkgs.mkShell {
  packages = [
    pkgs.git
    pkgs.gnumake
    pkgs.qemu
    pkgs.gdb

    pkgs.perl
    pkgs.python3

    riscv.gcc
    riscv.binutils
  ];

  shellHook = ''
    export NIX_CFLAGS_COMPILE="-std=gnu17 -Wno-error=stringop-truncation"
  '';
}