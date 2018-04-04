# ZyXeL VMG1312-B10A Buildroot + Baby Jumbo Frame patches

This is the buildroot provided by ZyXeL under their open-source programme.

I have applied patches on top in order to enable jumbo frames.

If you're lazy, you can go to "Releases" and grab a pre-rolled image from there.

## Building

It is *paramount* That you do this on a 32-bit version of Ubuntu 10.04, some of the crap in the buildroot fails horribly on other distributions and then builds an unbootable image. To make the VM able to grab updates, you have to edit /etc/apt/sources.list and replace the "xx.archive" part of the URLs with "old-releases".

1. Extract the toolchain tarball to the root of the build-machine's filesystem (the root folder of the tarball is opt and should basically be /opt on the machine)

2. Move/clone this repository to /tmp/consumer/bcm963xx\_router

3. `sudo dpkg-reconfigure dash` and say NO. (switches /bin/sh to /bin/bash)

4. `sudo apt-get install -y g++ flex bison gawk make autoconf zlib1g-dev libcurses-ocaml-dev libncurses-dev`

5. run `make PROFILE=VMG1312-B10A` and enjoy a drink while you wait.

## Flashing

You can flash images/ras.bin straight to your device from the device's Web UI.

## Version

These sources correspond to version 1.00(AAJZ.14)C0 of ZyXeL's binary build.

## License

My changes are provided under the GPL.

## Toolchain

Toolchain binaries are not provided for distribution, they are purely here _"unintentionally"_. You can get the source for it off ZyXeL anyway if you want it or I'll go bug them for sources - for a fee.
