COrg
====

COrg is a C implementation of the Organya music format used in Cave
Story by Daisuke "Pixel" Amaya.

The end goal is to be able to play any Organya song from the command
line with full support for both drum and melody tracks as well as
panning and volume features.

Details of the sample file and the .org format can be found in
doc/ORG_SPECS.txt.

I spent a small portion of a bus ride getting GPT 5.6 Sol to fix the issue with the drums, so after over a decade, this implementation finally works.

Building
--------

Requires libsdl and libsdl-mixer version 2

    $ make
