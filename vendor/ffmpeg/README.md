The `ffmpeg` directory contains a copy of chromium's (ffmpeg source)[https://chromium.googlesource.com/chromium/third_party/ffmpeg]
with the source code for unused codecs removed. The update.sh script automates
updating this.

`updatelcc.sh` updates LCC_REVISION with the libchromiumcontent revision used
by the current master of tofino-electron.

`update.sh` finds the ffmpeg revision used by LCC_REVISION and updates the source
code in `ffmpeg` to that version.

`patch` makes some minor modifications to the ffmpeg source, mainly adjusting
the build files to work without the rest of the chromium source.

The `tools` directory contains some other sources from the chromium tree that
are needed for building.
