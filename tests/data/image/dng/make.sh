#!/bin/sh
# Regenerates the lossless JPEG fixtures (ROADMAP 2d) with libjpeg-turbo 3's cjpeg and djpeg and
# ImageMagick 7. Each NAME.ljpg comes with NAME.pnm, djpeg's decoding of it (PGM or PPM, 16-bit
# big-endian when the precision is above 8), the reference our decoder is compared with.
# Sources are 37x29 (odd), random noise (every difference size up to 16 bits) with a smooth
# band (small differences), so that every code length and predictor is used.
set -e
cd "$(dirname "$0")"
source() {  # source DEPTH COLOURS OUT
    if [ "$2" = 1 ]; then
        magick -seed 11 -size 37x29 xc:gray +noise Random \
            \( -size 37x10 gradient:black-white \) -geometry +0+10 -composite \
            -colorspace gray -depth "$1" "$3"
    else
        magick -seed 11 -size 37x29 xc:gray +noise Random \
            \( -size 37x10 gradient:red-blue \) -geometry +0+10 -composite \
            -depth "$1" "$3"
    fi
}
source 16 1 gray16.pgm
source 12 1 gray12.pgm
source 8 1 gray8.pgm
source 14 3 rgb14.ppm
make() {  # make NAME INPUT PRECISION OPTIONS...
    name=$1 input=$2 precision=$3
    shift 3
    cjpeg -precision "$precision" "$@" "$input" > "$name.ljpg"
    djpeg -pnm "$name.ljpg" > "$name.pnm"
}
for p in 1 2 3 4 5 6 7; do
    make "gray16-p$p" gray16.pgm 16 -lossless "$p"
done
make gray12-p1-pt2 gray12.pgm 12 -lossless 1,2
make gray8-p7 gray8.pgm 8 -lossless 7
make gray16-restart gray16.pgm 16 -lossless 1 -restart 5
make rgb14-p1 rgb14.ppm 14 -rgb -lossless 1
make rgb14-p6-restart rgb14.ppm 14 -rgb -lossless 6 -restart 7
rm -f gray16.pgm gray12.pgm gray8.pgm rgb14.ppm
