COrg
====

COrg is a C implementation of the Organya music format used in Cave
Story by Daisuke "Pixel" Amaya.

The end goal is to be able to play any Organya song from the command
line with full support for both drum and melody tracks as well as
panning and volume features.

Details of the sample file and the .org format can be found in
doc/ORG_SPECS.txt.

Building
--------

Requires libsdl and libsdl-mixer version 2

    $ make

Known Issues
------------

Drum samples are not played back accurately.

I wrote this when I was in high school more than a decade ago.
Occasionally I dig it up but I don't know whether it will keep my
attention long enough to debug this issue.
