#! /bin/sh

cd `dirname $0`
SCRIPTS=`pwd`

# Get the libchromiumcontent version electron builds against
LCC=`python -c "from lib import config; print config.LIBCHROMIUMCONTENT_COMMIT"`

cd ..
SRC=`pwd`
FFMPEG=${SRC}/vendor/ffmpeg

# Find the real chrome version for this libchromiumcontent version
CV=`curl -fsSL https://raw.githubusercontent.com/electron/libchromiumcontent/${LCC}/VERSION`

# Find the revision of chromium's ffmpeg to use
REVISION=`curl -fsSL https://chromium.googlesource.com/chromium/src/+/${CV}/DEPS \
  | grep "ffmpeg.git" | sed -e s/.*ffmpeg\\.git@// -e s/\&#39\;.*//`

echo ${REVISION} >${FFMPEG}/FFMPEG_REVISION

rm -rf ${FFMPEG}/ffmpeg
mkdir ${FFMPEG}/ffmpeg
git checkout -- ${FFMPEG}/ffmpeg/README.mozilla
cd ${FFMPEG}/ffmpeg
curl -L https://chromium.googlesource.com/chromium/third_party/ffmpeg/+archive/${REVISION}.tar.gz | \
  tar xz --exclude=libavdevice --exclude=libavfilter --exclude=libavresample --exclude=libpostproc \
         --exclude=libswscale --exclude=doc --exclude=presets --exclude=tools \
         --exclude=tests --exclude="*/aarch64/*" --exclude="*/alpha/*" --exclude="*/arm/*" \
         --exclude="*/avr32/*" --exclude="*/mips/*" --exclude="*/neon/*" --exclude="*/ppc/*" \
         --exclude="*/sparc/*" --exclude="*/bfin/*" --exclude="*/sh4/*" --exclude="*/tomi/*" \
         --exclude="chromium/config/Chrome" --exclude="chromium/config/ChromeOS" \
         --exclude="chromium/config/ChromiumOS"

# Remove any source files that we don't want
git clean -f -X ${FFMPEG}/ffmpeg

# git clean doesn't handle directories with untracked files so well
rm -rf libswresample/aarch64 libswresample/arm libswresample/x86
rm -f ffmpeg/*.c ffmpeg/*.h ffmpeg/*.gni

# Clean up empty directories
find . -type d -empty -delete
