#!/bin/bash

#export CROSS_COMPILE=../prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64 
export LOCALVERSION=
export KDEB_PKGVERSION=5.10.160-1
make rockchip_linux_defconfig rockchip_linux_docker.config
make rk3588-blueberry-edge-v12-linux.img
#make bindeb-pkg -j16
