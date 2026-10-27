#!/bin/sh
# Regenerates the image decoding fixtures (ROADMAP 5.1) with ImageMagick 7 and exiftool.
# blocks.*: 48x32, six 16x16 blocks:
#   red    green  blue
#   white* black  yellow      (* half transparent where the format has alpha)
set -e
cd "$(dirname "$0")"
magick -size 16x16 xc:red xc:lime xc:blue +append \
    \( -size 16x16 'xc:rgba(255,255,255,0.5)' xc:black xc:yellow +append \) -append \
    -define png:color-type=6 PNG32:blocks.png
magick blocks.png -background white -alpha remove -alpha off opaque.ppm
magick opaque.ppm -quality 95 -sampling-factor 4:4:4 -strip blocks.jpg
cp blocks.jpg blocks-orient6.jpg
exiftool -q -overwrite_original -n -Orientation=6 blocks-orient6.jpg
magick opaque.ppm blocks.gif
magick blocks.png -define webp:lossless=true blocks.webp
magick blocks.png \( -size 48x32 xc:red \) -loop 0 -define webp:lossless=true \
    blocks-anim.webp
magick blocks.png -quality 100 blocks.jxl
head -c "$(($(stat -c %s blocks.jpg) / 2))" blocks.jpg > truncated.jpg
rm opaque.ppm
