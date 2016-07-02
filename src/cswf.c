/*
 * cswf - cswf.c
 * Copyright (c) 2016 Arkadiusz Bokowy
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lzma.h>
#include <zlib.h>


enum SWF_signature {
	/* standard uncompressed SWF file */
	SWF_SIGNATURE_F = 'F',
	/* data compressed using ZLIB; permitted in version >= 6 */
	SWF_SIGNATURE_C = 'C',
	/* data compressed using LZMA; permitted in version >= 13 */
	SWF_SIGNATURE_Z = 'Z',
};

struct __attribute__ ((packed)) SWF_header {
	uint8_t signature[3];
	uint8_t version;
	/* uncompressed SWF file length */
	uint32_t length;
};

struct __attribute__ ((packed)) SWF_header_ex {
	/* uint8_t frame_size[]; */
	uint16_t frame_rate;
	uint16_t frame_count;
};


static void *decompress_lzma(const struct SWF_header *header, const void *buffer, size_t size) {

	const struct __attribute__ ((packed)) LZMA_SWF_header {
		/* compressed SWF file length */
		uint32_t length;
		uint8_t properties[5];
	} *src_header = buffer;

	struct __attribute__ ((packed)) LZMA_7z_header {
		uint8_t properties[5];
		uint64_t decompressed_length;
	} lzma_header;

	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_auto_decoder(&strm, UINT64_MAX, 0) != LZMA_OK)
		return NULL;

	char *_buffer = malloc(header->length);
	strm.next_out = (void *)_buffer;
	strm.avail_out = header->length;

	memcpy(lzma_header.properties, src_header->properties, sizeof(lzma_header.properties));
	lzma_header.decompressed_length = header->length - sizeof(*header);

	strm.next_in = (uint8_t *)&lzma_header;
	strm.avail_in = sizeof(lzma_header);
	if (lzma_code(&strm, LZMA_RUN) != LZMA_OK)
		return NULL;

	strm.next_in = buffer + sizeof(*src_header);
	strm.avail_in = size - sizeof(*src_header);

	/* FIXME: For some unknown reasons, lzma_code returns LZMA_DATA_ERROR
	 *        even though decompressed data is *NOT* corrupted... */
	int code = lzma_code(&strm, LZMA_FINISH);
	fprintf(stderr, "debug: lzma_code -> %d\n", code);

	lzma_end(&strm);
	return _buffer;
}

static void *compress_lzma(const struct SWF_header *header, const void *buffer, size_t size) {
	return NULL;
}

static void *decompress_zlib(const struct SWF_header *header, const void *buffer, size_t size) {

	z_stream strm = { 0 };
	if (inflateInit(&strm) != Z_OK)
		return NULL;

	char *_buffer = malloc(header->length);
	strm.next_out = (void *)_buffer;
	strm.avail_out = header->length;

	strm.next_in = (void *)buffer;
	strm.avail_in = size;
	if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
		free(_buffer);
		_buffer = NULL;
	}

	inflateEnd(&strm);
	return _buffer;
}

static void *compress_zlib(const struct SWF_header *header, const void *buffer, size_t size) {
	return NULL;
}

int main(int argc, char *argv[]) {

	int opt;
	const char *opts = "hdcz";
	struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "decompress", no_argument, NULL, 'd' },
		{ "compress", no_argument, NULL, 'c' },
		{ "zcompress", no_argument, NULL, 'z' },
		{ 0, 0, 0, 0 },
	};

	int compress = 0;
	int decompress = 0;
	/* use lzma compression, otherwise zlib */
	int use_lzma = 0;

	/* parse options */
	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'h':
return_usage:
			printf("usage: %s [ -cdhz ] [ filename ]\n", argv[0]);
			return EXIT_SUCCESS;

		case 'd':
			decompress = 1;
			break;
		case 'c':
			compress = 1;
			break;
		case 'z':
			compress = 1;
			use_lzma = 1;
			break;

		default:
			fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}

	const int is_atty = isatty(fileno(stdin));
	FILE *f_swf = stdin;

	if (is_atty) {

		/* if running from terminal filename is required */
		if (optind == argc)
			goto return_usage;

		if ((f_swf = fopen(argv[optind], "r")) == NULL) {
			fprintf(stderr, "%s: %s: %s\n", argv[0], argv[optind], strerror(errno));
			return EXIT_FAILURE;
		}

	}

	struct SWF_header header;
	unsigned char *buffer;
	size_t size;

	if (fread(&header, sizeof(header), 1, f_swf) != 1) {
			fprintf(stderr, "%s: unable to read SWF header\n", argv[0]);
			return EXIT_FAILURE;
	}

	/* test whatever we are reading a SWF file - check magic number */
	if ((header.signature[0] != SWF_SIGNATURE_F && header.signature[0] != SWF_SIGNATURE_C &&
				header.signature[0] != SWF_SIGNATURE_Z) || header.signature[1] != 'W' || header.signature[2] != 'S') {
		fprintf(stderr, "%s: not a SWF format or data corrupted\n", argv[0]);
		return EXIT_FAILURE;
	}

	buffer = malloc(header.length);
	size = fread(buffer, 1, header.length, f_swf);
	if (ferror(f_swf)) {
		fprintf(stderr, "%s: error occurred during data read\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (is_atty)
		fclose(f_swf);

	if (header.signature[0] != SWF_SIGNATURE_F) {
		/* decompress data - part of the SWF header is in the compressed area, so
		 * we need this data for compression/decompression or dumping header */

		unsigned char *tmp;

		if (header.signature[0] == SWF_SIGNATURE_C)
			tmp = decompress_zlib(&header, buffer, size);
		else if (header.signature[0] == SWF_SIGNATURE_Z)
			tmp = decompress_lzma(&header, buffer, size);

		if (tmp == NULL) {
			fprintf(stderr, "%s: data decompression failed\n", argv[0]);
			return EXIT_FAILURE;
		}

		free(buffer);
		buffer = tmp;

	}

	if (compress || decompress) {

		f_swf = stdout;
		header.signature[0] = SWF_SIGNATURE_F;

		if (compress) {

			unsigned char *tmp;

			if (use_lzma) {
				header.signature[0] = SWF_SIGNATURE_Z;
				if (header.version < 13)
					fprintf(stderr, "%s: warning: using LZMA compression for SWF version < 13\n", argv[0]);
				tmp = compress_lzma(&header, buffer, size);
			}
			else {
				header.signature[0] = SWF_SIGNATURE_C;
				if (header.version < 6)
					fprintf(stderr, "%s: warning: using ZLIB compression for SWF version < 6\n", argv[0]);
				tmp = compress_zlib(&header, buffer, size);
			}

			if (tmp == NULL) {
				fprintf(stderr, "%s: data compression failed\n", argv[0]);
				return EXIT_FAILURE;
			}

			free(buffer);
			buffer = tmp;

		}

		if (is_atty && (f_swf = fopen(argv[optind], "w")) == NULL) {
			fprintf(stderr, "%s: unable to write file: %s\n", argv[0], strerror(errno));
			return EXIT_FAILURE;
		}

		/* write converted (compressed or decompressed) SWF file */
		fwrite(&header, sizeof(header), 1, f_swf);
		fwrite(buffer, header.length - sizeof(header), 1, f_swf);
		fclose(f_swf);

	}
	else {
		/* dump information stored in the header */

		printf("Adobe Flash, version %u - %.3s%s\n", header.version, header.signature,
				header.signature[0] == SWF_SIGNATURE_F ? "" : " (compressed)");
		printf("Data size: %u bytes\n", header.length);

		unsigned int rect[4] = { 0 };
		int i, n, bit = 5, nbits = buffer[0] >> 3;

		/* get frame size from the RECT field */
		for (n = 0; n < 4; n++) {
			for (i = nbits; i; i--) {
				if ((buffer[0] << bit) & 0x80)
					rect[n] |= 1 << (i - 1);
				if (++bit >= 8) {
					buffer++;
					bit = 0;
				}
			}
		}

		struct SWF_header_ex *header_ex = (struct SWF_header_ex *)&buffer[1];

		printf("Frame size: %d x %d\n", rect[1] / 20, rect[3] / 20);
		printf("Frames: %u\n", header_ex->frame_rate);
		printf("FPS: %u\n", header_ex->frame_count);

	}

	/* NOTE: Allocated memory and opened resources will be released by the
	 *       underlying operating system. There is no need to bother. */
	return EXIT_SUCCESS;
}
