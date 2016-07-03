#!/bin/sh

SWFPACK=../src/swfpack

cp $srcdir/black-and-white.swf black-and-white-zlib.swf
cp $srcdir/black-and-white.swf black-and-white-lzma.swf
$SWFPACK -c black-and-white-zlib.swf
$SWFPACK -z black-and-white-lzma.swf

cp $srcdir/black-and-white.md5 black-and-white.md5
md5sum -c black-and-white.md5
