# EDGESec
[![C/C++ CI](https://github.com/nqminds/EDGESec/workflows/C/C++%20CI/badge.svg?branch=main)](https://github.com/nqminds/EDGESec/actions?query=workflow%3A%22Github+Pages%22)

## Build

Instructions to create `.deb` file are located in
[`./docs/CREATING_A_DEB.md`](./docs/CREATING_A_DEB.md).

### Installing Dependencies

On Ubuntu, we need a C compiler, CMake, Doxygen, and libnl libraries:

```bash
sudo apt update
build_dependencies=(
    cmake # build-tool
    git # required to download dependencies
    ca-certificates # required for git+https downloads
    doxygen texinfo graphviz # documentation
    build-essential # C and C++ compilers
    libnl-genl-3-dev libnl-route-3-dev # netlink dependencies
    automake # required by libmicrohttpd for some reason?
    autopoint gettext # required by libuuid
    autoconf # required by compile_sqlite.sh
    libtool-bin # required by autoconf somewhere
    pkg-config # seems to be required by nDPI
    libjson-c-dev # mystery requirement
    flex bison # required by pcap
    libssl-dev # required by hostapd only. GRPC uses own version, and we compile OpenSSL 3 for EDGESec
    libmnl0 # we compile our own version of mnl, but we have a linking issue, so temporarily install a system version
    protobuf-compiler-grpc libprotobuf-dev libgrpc++-dev # GRPC, can be removed if -DBUILD_GRPC_LIB=ON
    libcmocka-dev # cmocka, can be removed if -DBUILD_CMOCKA_LIB=ON
)
runtime_dependencies=(
    dnsmasq
    jq # required by predictable wifi name script
)
sudo apt install -y "${build_dependencies[@]}" "${runtime_dependencies[@]}"
```

### Compile

Compiling EDGESec is done with CMake.

First, configure `cmake` in the `build/` directory by running the following.
(equivalent to `mkdir build && cd build && cmake ..`)
This is currently very slow, as it compiles all the C programs single-core.

```bash
cmake -B build/ -S .
```

To build, you can then run (`-j4` means 4 jobs/threads, replace `4` with the amount of cores you want to use):
(equivalent to `make -j4`)

```bash
cmake --build build/ -j4
```

After succesful compilation the binary will be located in ```./build/src``` folder.

You can use the following to also install files into `build/edgesec-dist` (equivalent to `make install`):

```bash
cmake --build build/ --target install -j4
```

## Running

To run ```edgesec``` tool with the configuration file ```dev-config.ini``` located in ```./build``` folder use:

```console
./build/src/edgesec -c ./build/dev-config.ini
```

To enable verbose debug mode use:
```console
./build/src/edgesec -c ./build/dev-config.ini -ddddd
```

The configuration file `config.ini` has been setup to work by default only when:
  - running on Raspberry Pi (e.g. `wlan1` is the name of Wifi USB AP and `eth0` is the ethernet port)
  - running after `make install` has been run

## Testing

To compile the tests use:

```bash
cmake -B build/ -S .
cmake --build build/ --target test -j4 # or `make test`
```

To run each test individually the test binaries can be located in ```./build/tests``` folder.

## Developer Documentation

To compile the docs from ```./build``` folder:
```console
make doxydocs
```

See [`./docs`](./docs) for how to build the developer doxygen documentation website.

## Config
[Configuration file structure](./docs/CONFIG.md)

## Commands
[Hostapd and supervisor commands](./docs/COMMANDS.md)

## ISSUES
[Installation and compilation issues](./docs/ISSUES.md)
