#!/bin/sh
# Regenerates the image decoding and thumbnail fixtures (ROADMAP 5.1, 5.2, 5.4) with ImageMagick 7 and exiftool.
# blocks.*: 48x32, six 16x16 blocks:
#   red    green  blue
#   white* black  yellow      (* half transparent where the format has alpha)
set -e
cd "$(dirname "$0")"
magick -size 16x16 xc:red xc:lime xc:blue +append \
    \( -size 16x16 'xc:rgba(255,255,255,0.5)' xc:black xc:yellow +append \) -append \
    -define png:color-type=6 -define png:exclude-chunks=date,time PNG32:blocks.png
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
# 480x320 (the blocks, ten times larger) carrying a 192x128 copy as its EXIF thumbnail,
# orientation 6.
magick opaque.ppm -scale 1000% -quality 90 -sampling-factor 4:4:4 -strip with-thumbnail.jpg
magick opaque.ppm -scale 400% -quality 90 -sampling-factor 4:4:4 -strip thumbnail.jpg
exiftool -q -overwrite_original -n '-ThumbnailImage<=thumbnail.jpg' -Orientation=6 \
    with-thumbnail.jpg
rm thumbnail.jpg
# 96x64, uncompressed 8-bit RGB in strips of 7 rows (the last one short), marked as a
# reduced-resolution image: exiv2 lists such an IFD0 as a TIFF preview, as in Hasselblad's 3FR.
magick opaque.ppm -scale 200% -depth 8 -compress none -define tiff:rows-per-strip=7 \
    tiff-preview.tif
exiftool -q -overwrite_original -n -SubfileType=1 tiff-preview.tif
rm opaque.ppm
