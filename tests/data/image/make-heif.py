#!/usr/bin/env python3
"""Regenerate original HEIC fixtures with heif-enc 1.23+ (x265), lcms2 and Python 3.
Run inside the dev shell with heif-enc on PATH. No external photographs are used.
"""
import ctypes as c
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parent


def png(path, width, height, pixel, depth=8, icc=None):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            for v in pixel(x, y):
                rows.extend(bytes([v]) if depth == 8 else struct.pack('>H', v))
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, depth, 6, 0, 0, 0))
    if icc:
        data += chunk(b'iCCP', b'Display P3\0\0' + zlib.compress(icc))
    path.write_bytes(data + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def encode(source, name, *options):
    subprocess.run(['heif-enc', '-L', '-p', 'x265:pools=none', *options,
                    str(source), '-o', str(ROOT / name)], check=True)


def p3_profile():
    libdir = subprocess.check_output(['pkg-config', '--variable=libdir', 'lcms2'], text=True).strip()
    lib = c.CDLL(str(Path(libdir) / 'liblcms2.so'))
    class xyY(c.Structure):
        _fields_ = [('x', c.c_double), ('y', c.c_double), ('Y', c.c_double)]
    class Triple(c.Structure):
        _fields_ = [('r', xyY), ('g', xyY), ('b', xyY)]
    lib.cmsBuildParametricToneCurve.argtypes = [c.c_void_p, c.c_int, c.POINTER(c.c_double)]
    lib.cmsBuildParametricToneCurve.restype = c.c_void_p
    lib.cmsCreateRGBProfile.argtypes = [c.POINTER(xyY), c.POINTER(Triple), c.POINTER(c.c_void_p)]
    lib.cmsCreateRGBProfile.restype = c.c_void_p
    lib.cmsSaveProfileToMem.argtypes = [c.c_void_p, c.c_void_p, c.POINTER(c.c_uint32)]
    lib.cmsCloseProfile.argtypes = [c.c_void_p]
    lib.cmsFreeToneCurve.argtypes = [c.c_void_p]
    curve = lib.cmsBuildParametricToneCurve(None, 4, (c.c_double * 5)(2.4, 1 / 1.055, .055 / 1.055, 1 / 12.92, .04045))
    profile = lib.cmsCreateRGBProfile(c.byref(xyY(.3127, .3290, 1)),
        c.byref(Triple(xyY(.68, .32, 1), xyY(.265, .69, 1), xyY(.15, .06, 1))),
        (c.c_void_p * 3)(curve, curve, curve))
    size = c.c_uint32()
    lib.cmsSaveProfileToMem(profile, None, c.byref(size))
    data = c.create_string_buffer(size.value)
    lib.cmsSaveProfileToMem(profile, data, c.byref(size))
    lib.cmsCloseProfile(profile)
    lib.cmsFreeToneCurve(curve)
    return data.raw


encode(ROOT / 'blocks.png', 'blocks.heic')
encode(ROOT / 'blocks.png', 'blocks-rotated.heic', '--rotate-cw', '90')
encode(ROOT / 'blocks.png', 'blocks-mirrored.heic', '--flip-h')
with tempfile.TemporaryDirectory() as folder:
    source = Path(folder) / 'input.png'
    png(source, 32, 32, lambda x, y: (128, 0, 0, 128) if x < 16 else (0, 0, 0, 0))
    encode(source, 'premultiplied.heic', '--premultiplied-alpha')
    # Odd dimensions require clean aperture crop after codec padding.
    png(source, 47, 31, lambda x, y: (255 if x < 24 else 0, 0, 255 if x >= 24 else 0, 255))
    encode(source, 'crop.heic')
    # Large grid with a partial right/bottom tile, like phone HEIC grids.
    png(source, 1537, 1025, lambda x, y: (255 if x < 768 else 0, 255 if y >= 512 else 0, 0, 255))
    encode(source, 'grid.heic', '--cut-tiles', '512')
    # Interior-gamut colour: P3 (180,100,60) should become approximately sRGB (193,95,47).
    png(source, 32, 32, lambda x, y: (180, 100, 60, 128), icc=p3_profile())
    encode(source, 'p3.heic', '--enable-two-colr-boxes')
    png(source, 32, 32, lambda x, y: (180, 100, 60, 128))
    encode(source, 'p3-nclx.heic', '--colour_primaries', '12', '--transfer_characteristic', '13')
    # 10-bit PQ/BT.2020 grayscale ramp, black / 100 nits / 1000 nits / peak white.
    levels = [0, 33297, 49271, 65535]
    png(source, 64, 32, lambda x, y: (levels[x // 16],) * 3 + (65535,), depth=16)
    encode(source, 'pq.heic', '-b', '10', '--colour_primaries', '9', '--transfer_characteristic', '16')
    encode(source, 'hlg.heic', '-b', '10', '--colour_primaries', '9', '--transfer_characteristic', '18')

    # Valid AV1 in generic HEIF: the reader intentionally ships only HEVC decoding.
    avif = Path(folder) / 'unsupported.avif'
    subprocess.run(['heif-enc', '-A', '-L', str(ROOT / 'blocks.png'), '-o', str(avif)], check=True)
    data = bytearray(avif.read_bytes())
    data[8:12] = b'mif1'
    (ROOT / 'unsupported-codec.heif').write_bytes(data)
