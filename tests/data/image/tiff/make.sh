#!/bin/sh
# Regenerates the TIFF fixtures (ROADMAP 2c) with ImageMagick 7 (libtiff inside) and libtiff's
# tools. Each NAME.tif comes with NAME.png: the same picture as ImageMagick decodes it, 8-bit
# RGBA, the reference our decoder is compared with.
# source.png: 37x29 (odd, so that strips and tiles end short), colourful, alpha from 255 at the
# left to 64 at the right.
set -e
cd "$(dirname "$0")"
magick -seed 7 -size 37x29 plasma:fractal \
    \( -size 29x37 gradient:white-'gray(25%)' -rotate 90 \) \
    -alpha off -compose copy-opacity -composite \
    -define png:color-type=6 -define png:exclude-chunks=date,time PNG32:source.png
magick source.png -background white -alpha remove -alpha off opaque.png
make() {  # make NAME INPUT OPTIONS...
    name=$1 input=$2
    shift 2
    magick "$input" "$@" "$name.tif"
    magick "$name.tif[0]" -depth 8 -define png:color-type=6 \
        -define png:exclude-chunks=date,time PNG32:"$name.png"
}
make rgb8-none opaque.png -depth 8 -compress none
make rgb8-lzw-predictor opaque.png -depth 8 -compress lzw -define tiff:predictor=2
make rgb8-zip-predictor opaque.png -depth 8 -compress zip -define tiff:predictor=2
make rgb8-packbits opaque.png -depth 8 -compress rle
make rgb8-strips opaque.png -depth 8 -compress lzw -define tiff:rows-per-strip=5
make rgb8-big-endian opaque.png -depth 8 -compress lzw -endian MSB
make rgba8-lzw source.png -depth 8 -compress lzw
make rgba8-associated source.png -depth 8 -compress zip -define tiff:alpha=associated
make rgb16-zip-predictor opaque.png -depth 16 -compress zip -define tiff:predictor=2
make rgb16-big-endian opaque.png -depth 16 -compress none -endian MSB
make gray8-lzw opaque.png -colorspace gray -depth 8 -compress lzw
make gray16-none opaque.png -colorspace gray -depth 16 -compress none
make gray4-none opaque.png -colorspace gray -depth 4 -compress none
make gray1-none opaque.png -colorspace gray -threshold 50% -depth 1 -compress none
make gray1-white opaque.png -colorspace gray -threshold 50% -depth 1 -compress none \
    -define tiff:photometric=min-is-white
make palette8-lzw opaque.png -colors 200 -type palette -compress lzw
make cmyk8-lzw opaque.png -colorspace cmyk -depth 8 -compress lzw
make float32-zip opaque.png -define quantum:format=floating-point -depth 32 -compress zip
make float32-predictor opaque.png -define quantum:format=floating-point -depth 32 \
    -compress zip -define tiff:predictor=3
make float16-zip opaque.png -define quantum:format=floating-point -depth 16 -compress zip
make tiled-lzw opaque.png -depth 8 -compress lzw -define tiff:tile-geometry=16x16
make tiled-zip-predictor source.png -depth 16 -compress zip -define tiff:predictor=2 \
    -define tiff:tile-geometry=16x16
make planar-zip opaque.png -depth 8 -compress zip -interlace plane
make planar-tiled source.png -depth 8 -compress lzw -interlace plane \
    -define tiff:tile-geometry=16x16
make orient6 opaque.png -depth 8 -compress lzw -orient right-top
# ImageMagick adds the horizontal predictor to LZW and Deflate on its own: libtiff's tiffcp
# writes them without one.
tiffcp -c lzw:1 rgb8-none.tif rgb8-lzw.tif
tiffcp -c zip:1 rgb8-none.tif rgb8-zip.tif
for name in rgb8-lzw rgb8-zip; do
    magick "$name.tif" -depth 8 -define png:color-type=6 -define png:exclude-chunks=date,time \
        PNG32:"$name.png"
done
make two-pages opaque.png -depth 8 -compress lzw \( source.png -rotate 90 \)
magick opaque.png -depth 8 -compress lzw TIFF64:bigtiff-lzw.tif
magick bigtiff-lzw.tif -depth 8 -define png:color-type=6 -define png:exclude-chunks=date,time \
    PNG32:bigtiff-lzw.png
# JPEG inside TIFF: ImageMagick writes it with RGB photometric, libtiff's tiffcp with YCbCr
# 4:2:0 (as Photoshop and most cameras do).
make jpeg-rgb opaque.png -compress jpeg -quality 90 -sampling-factor 2x2
make jpeg-rgb-444 opaque.png -compress jpeg -quality 90 -sampling-factor 1x1
make jpeg-gray opaque.png -colorspace gray -compress jpeg -quality 90
make jpeg-tiled opaque.png -compress jpeg -quality 90 -define tiff:tile-geometry=16x16
make jpeg-strips opaque.png -compress jpeg -quality 90 -define tiff:rows-per-strip=16
magick opaque.png -depth 8 -compress none ycbcr-source.tif
tiffcp -c jpeg:90 ycbcr-source.tif jpeg-ycbcr.tif
tiffcp -c jpeg:90 -t -w 16 -l 16 ycbcr-source.tif jpeg-ycbcr-tiled.tif
rm ycbcr-source.tif
for name in jpeg-ycbcr jpeg-ycbcr-tiled; do
    magick "$name.tif" -depth 8 -define png:color-type=6 -define png:exclude-chunks=date,time \
        PNG32:"$name.png"
done
# Broken on purpose: cut in the middle of its strips, and the same with an offset past the end.
head -c 1500 rgb8-lzw.tif > broken-truncated.tif
# broken-tile-tag.tif and broken-exiv2-overflow.tif are not made here: the fuzzers found them (a
# TileWidth tag slipped into a TIFF of strips; a file whose previews make exiv2 throw
# std::overflow_error), kept as they were.
rm opaque.png
