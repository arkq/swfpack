SWF compressor/decompressor [![Build Status](https://travis-ci.org/Arkq/swfpack.svg?branch=master)](https://travis-ci.org/Arkq/swfpack)
===========================

Swfpack provides a convenient way of compressing and decompressing [Adobe
Flash](https://en.wikipedia.org/wiki/Adobe_Flash) files also known as Macromedia Flash. This
utility can act as an extension for the [SWFTools](http://www.swftools.org/) - the collection of
utilities for working with Adobe Flash files.


Installation
------------

	$ autoreconf --install
	$ mkdir build && cd build
	$ ../configure
	$ make && make install


Usage
-----

	$ swfpack -h
	usage: swfpack [ -cdhz ] [ filename ]
	  -h, --help            print this help and exit
	  -d, --decompress      decompress given SWF file
	  -c, --compress        compress SWF file using DEFLATE algorithm
	  -z, --zcompress       compress SWF file using LZMA algorithm

In place compression/decompression:

	$ swfpack -c uncompressed.swf

or

	$ swfpack -d lzma-compressed.swf

Swfpack can also read SWF file content from the standard input and write processed file to the
standard output. By this way of processing, one can use swfpack as a part of processing toolchain,
e.g.:

	$ curl http://www.swftools.org/flash/box.swf |swfpack -z >box.swf


Similar projects
----------------

1. [swfzip](https://github.com/OpenGG/swfzip) - python script for compressing/decompressing SWF
	 files
2. [xxxswf](https://bitbucket.org/Alexander_Hanel/xxxswf) - another python script for manipulating
	 SWF files
