#! /bin/bash
# Run in Nios V Command Shell, Quartus Prime 22.4 or later

quartus_project=$1
qsys_file=$2
hex_file=$3

usage()
{
    echo "Usage:"
    echo "    build.sh <quartus_project_file> <qsys_file> <destination_hex_file>"
}

if [ -z "$quartus_project" ]; then
    usage
    exit 1
fi

if [ -z "$qsys_file" ]; then
    usage
    exit 1
fi

if [ -z "$hex_file" ]; then
    usage
    exit 1
fi

if [ ! -f "$quartus_project" ]; then
    echo Quartus project file not found "$quartus_project"
    usage
    exit 1
fi

if [ ! -f "$qsys_file" ]; then
    echo qsys file not found "$qsys_file"
    usage
    exit 1
fi

# Export the bsp folder from the Quartus project, create the
# CMakeFiles.txt for the application, build the app, then
# build the stream_controller.hex binary, in the 'build' folder

niosv-bsp -c --quartus-project=$quartus_project --qsys=$qsys_file --type=hal bsp/settings.bsp
niosv-app --bsp-dir=bsp --app-dir=app --srcs=app --elf-name=stream_controller.elf

# cmake dependency, version 3.14.10 or later. https://cmake.org/download/
cmake -B build -DCMAKE_BUILD_TYPE=Release app
cmake --build build
elf2hex build/stream_controller.elf -b 0x0 -w 32 -e 0x1ffff -r 4 -o build/stream_controller.hex
cp build/stream_controller.hex $hex_file

exit 0
