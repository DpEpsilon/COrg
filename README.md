COrg
====

COrg is a C implementation of the Organya music format used in Cave
Story by Daisuke "Pixel" Amaya.

The end goal is to be able to play any Organya song from the command
line with full support for both drum and melody tracks as well as
panning and volume features.

Details of the sample file and the .org format can be found in
doc/ORG_SPECS.txt.

You may have come across this implementation before, and it was a bit
broken and lacking certain features. Now with the help of GPT 5.6 Sol
and Claude Opus 5.5, it's working quite well and pretty feature
complete. I originally wrote this in high school and wasn't really
interested in sinking too much time into fixing the drum issue.

Building
--------

Requires SDL 2

    $ make

### Windows (INSTRUCTIONS UNTESTED!!!)

`make windows` builds a self-contained corg.exe with SDL 2 linked
statically, so no DLLs need to be shipped alongside it. SDL2_MINGW
points at a directory containing SDL 2's include/ and lib/libSDL2.a.

On Windows, install MSYS2 (https://www.msys2.org), open the
"MSYS2 UCRT64" shell, and install the compiler, SDL 2 and make:

    $ pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 make

Then, from the COrg directory:

    $ make windows WIN_CC=gcc SDL2_MINGW=/ucrt64

To cross-compile from Linux, install MinGW-w64 (mingw64-gcc on
Fedora), download SDL2-devel-2.x.x-mingw.tar.gz from
https://github.com/libsdl-org/SDL/releases, extract it, and run:

    $ make windows SDL2_MINGW=path/to/SDL2-2.x.x/x86_64-w64-mingw32

corg.exe looks for orgsamp.dat in the current directory, so run it
from the folder that contains it.

Running
-------

    $ ./corg [--lowpass|-l] path/to/song.org

The --lowpass/-l flag tells COrg to apply a fixed lowpass filter to
soften the harsh square and sawtooth waves.
