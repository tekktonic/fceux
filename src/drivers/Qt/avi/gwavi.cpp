/*
 * Copyright (c) 2008-2011, Michael Kohn
 * Copyright (c) 2013, Robin Hahling
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the author nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is the file containing gwavi library functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gwavi.h"

/**
 * This is the first function you should call when using gwavi library.
 * It allocates memory for a gwavi_t structure and returns it and takes care of
 * initializing the AVI header with the provided information.
 *
 * When you're done creating your AVI file, you should call gwavi_close()
 * function to free memory allocated for the gwavi_t structure and properly
 * close the output file.
 *
 * @param filename This is the name of the AVI file which will be generated by
 * this library.
 * @param width Width of a frame.
 * @param height Height of a frame.
 * @param fourcc FourCC representing the codec of the video encoded stream. a
 * FourCC is a sequence of four chars used to uniquely identify data formats.
 * For more information, you can visit www.fourcc.org.
 * @param fps Number of frames per second of your video. It needs to be > 0.
 * @param audio This parameter is optionnal. It is used for the audio track. If
 * you do not want to add an audio track to your AVI file, simply pass NULL for
 * this argument.
 *
 * @return Structure containing required information in order to create the AVI
 * file. If an error occured, NULL is returned.
 */

gwavi_t::gwavi_t(void)
{
	in = out = NULL;
	memset( &avi_header     , 0, sizeof(struct gwavi_header_t) );
	memset( &stream_header_v, 0, sizeof(struct gwavi_stream_header_t) );
	memset( &stream_format_v, 0, sizeof(struct gwavi_stream_format_v_t) );
	memset( &stream_header_a, 0, sizeof(struct gwavi_stream_header_t) );
	memset( &stream_format_a, 0, sizeof(struct gwavi_stream_format_a_t) );
	memset( &stream_index_v , 0, sizeof(struct gwavi_super_indx_t) );
	memset( &stream_index_a , 0, sizeof(struct gwavi_super_indx_t) );
	memset(  fourcc, 0, sizeof(fourcc) );
	std_index_base_ofs_v = 0;
	std_index_base_ofs_a = 0;
	marker = 0;
	movi_fpos = 0;
	bits_per_pixel = 24;
	avi_std = 2;
	audioEnabled = false;
}

gwavi_t::~gwavi_t(void)
{
	if ( in != NULL )
	{
		fclose(in); in = NULL;
	}
	if ( out != NULL )
	{
		fclose(out); out = NULL;
	}

}

int
gwavi_t::openIn(const char *filename)
{
	if ((in = fopen(filename, "rb")) == NULL) 
	{
		perror("gwavi_open: failed to open file for reading");
		return -1;
	}
	return 0;
}


int
gwavi_t::open(const char *filename, unsigned int width, unsigned int height,
	   const char *fourcc, double fps, struct gwavi_audio_t *audio)
{
	int size = 0;
	unsigned int usec;

	memset( this->fourcc, 0, sizeof(this->fourcc) );
	strcpy( this->fourcc, fourcc );

	if (check_fourcc(fourcc) != 0)
	{
		(void)fprintf(stderr, "WARNING: given fourcc does not seem to "
			      "be valid: %s\n", fourcc);
	}

	if (fps < 1)
	{
		return -1;
	}
	if ((out = fopen(filename, "wb+")) == NULL) 
	{
		perror("gwavi_open: failed to open file for writing");
		return -1;
	}
	usec = (unsigned int)((1000000.0 / fps)+0.50);
	printf("FPS: %f  %u\n", fps, usec );

	/* set avi header */
	avi_header.time_delay= usec;
	avi_header.data_rate = width * height * 3 * (((unsigned int)fps)+1);
	avi_header.flags = 0x10;

	if (audio)
	{
		avi_header.data_streams = 2;
	}
	else
	{
		avi_header.data_streams = 1;
	}

	if ( strcmp( fourcc, "I420" ) == 0 )
	{  // I420   YUV 4:2:0
		bits_per_pixel = 12;
	}
	else if ( strcmp( fourcc, "X264" ) == 0 )
	{  // X264   H.264
		bits_per_pixel = 12;
	}
	else if ( strcmp( fourcc, "H265" ) == 0 )
	{  // X265   H.265
		bits_per_pixel = 12;
	}
	else
	{	// Plain RGB24
		bits_per_pixel = 24;
	}
	size = (width * height * bits_per_pixel);

	if ( (size % 8) != 0 )
	{
		printf("Warning: Video Buffer Size not on an 8 bit boundary: %ux%u:%i\n", width, height, bits_per_pixel);
	}
	size = size / 8;

	/* this field gets updated when calling gwavi_close() */
	avi_header.number_of_frames = 0;
	avi_header.width = width;
	avi_header.height = height;
	avi_header.buffer_size = size;

	/* set stream header */
	(void)strcpy(stream_header_v.data_type, "vids");
	(void)memcpy(stream_header_v.codec, fourcc, 4);
	stream_header_v.time_scale = usec;
	stream_header_v.data_rate = 1000000;
	stream_header_v.buffer_size = size;
	stream_header_v.data_length = 0;
	stream_header_v.image_width  = width;
	stream_header_v.image_height = height;

	/* set stream format */
	stream_format_v.header_size = 40;
	stream_format_v.width = width;
	stream_format_v.height = height;
	stream_format_v.num_planes = 1;
	stream_format_v.bits_per_pixel = bits_per_pixel;
	stream_format_v.compression_type =
		((unsigned int)fourcc[3] << 24) +
		((unsigned int)fourcc[2] << 16) +
		((unsigned int)fourcc[1] << 8) +
		((unsigned int)fourcc[0]);
	stream_format_v.image_size = size;
	stream_format_v.colors_used = 0;
	stream_format_v.colors_important = 0;

	stream_format_v.palette = 0;
	stream_format_v.palette_count = 0;

	strcpy( stream_index_v.chunkId, "00dc");
	stream_index_v.streamId = 0;

	audioEnabled = false;

	if (audio) 
	{
		/* set stream header */
		memcpy(stream_header_a.data_type, "auds", 4);
		stream_header_a.codec[0] = 1;
		stream_header_a.codec[1] = 0;
		stream_header_a.codec[2] = 0;
		stream_header_a.codec[3] = 0;
		stream_header_a.time_scale = 1;
		stream_header_a.data_rate = audio->samples_per_second;
		stream_header_a.buffer_size =
			audio->channels * (audio->bits / 8) * audio->samples_per_second;
		/* when set to -1, drivers use default quality value */
		stream_header_a.audio_quality = -1;
		stream_header_a.sample_size =
			(audio->bits / 8) * audio->channels;

		/* set stream format */
		stream_format_a.format_type = 1;
		stream_format_a.channels = audio->channels;
		stream_format_a.sample_rate = audio->samples_per_second;
		stream_format_a.bytes_per_second =
			audio->channels * (audio->bits / 8) * audio->samples_per_second;
		stream_format_a.block_align =
			audio->channels * (audio->bits / 8);
		stream_format_a.bits_per_sample = audio->bits;
		stream_format_a.size = 0;

		strcpy( stream_index_a.chunkId, "01wb");
		stream_index_a.streamId = 1;
		audioEnabled = true;
	}
	std_index_base_ofs_v = 0;
	std_index_base_ofs_a = 0;

	if (write_chars_bin(out, "RIFF", 4) == -1)
		goto write_chars_bin_failed;
	if (write_int(out, 0) == -1) {
		(void)fprintf(stderr, "gwavi_info: write_int() failed\n");
		return -1;
	}
	if (write_chars_bin(out, "AVI ", 4) == -1)
		goto write_chars_bin_failed;

	if (write_avi_header_chunk(out) == -1) {
		(void)fprintf(stderr, "gwavi_info: write_avi_header_chunk "
			      "failed\n");
		return -1;
	}

	if (write_chars_bin(out, "LIST", 4) == -1)
		goto write_chars_bin_failed;
	if ((marker = ftell(out)) == -1) {
		perror("gwavi_info (ftell)");
		return -1;
	}
	if (write_int(out, 0) == -1) {
		(void)fprintf(stderr, "gwavi_info: write_int() failed\n");
		return -1;
	}
	movi_fpos = ftell(out);

	if (write_chars_bin(out, "movi", 4) == -1)
		goto write_chars_bin_failed;

	// Reserve space for about 4 hours of offsets
	// 2 streams * 4 hours * 60 fps * 3600 seconds per hour.
	offsets.reserve( 2 * 4 * 60 * 3600 ); 

	return 0;

write_chars_bin_failed:
	(void)fprintf(stderr, "gwavi_open: write_chars_bin() failed\n");
	return -1;
}

/**
 * This function allows you to add an encoded video frame to the AVI file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param buffer Video buffer size.
 * @param len Video buffer length.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::add_frame( unsigned char *buffer, size_t len, unsigned int flags)
{
	size_t t, maxi_pad;  /* if your frame is raggin, give it some paddin' */
	gwavi_index_rec_t idx;
	long long fpos;

	if ( !buffer)
	{
		(void)fputs("gwavi and/or buffer argument cannot be NULL",
			    stderr);
		return -1;
	}
	fpos = ftell(out);

	if ( std_index_base_ofs_v == 0 )
	{
		std_index_base_ofs_v = fpos;
	}
	fpos = fpos - std_index_base_ofs_v;

	if ( fpos > 0x7FFFFFFF)
	{
		//printf("STD Index Page Reset\n");
		if ( avi_std >= 2 )
		{
			if ( write_stream_std_indx( out, &stream_index_v ) == -1 )
			{
				return -1;
			}

			if ( audioEnabled )
			{
				if ( write_stream_std_indx( out, &stream_index_a ) == -1 )
				{
					return -1;
				}
			}
			offsets.clear();

			std_index_base_ofs_v = 0;
			std_index_base_ofs_a = 0;
		}
	}
	
	stream_header_v.data_length++;

	maxi_pad = len % WORD_SIZE;
	if (maxi_pad > 0)
	{
		maxi_pad = WORD_SIZE - maxi_pad;
	}

	//printf("Frame Offset: %li\n", ftell(out) - movi_fpos );

	idx.fofs     = ftell(out);
	idx.len      = len;
	idx.type     = 0;
	idx.keyFrame = (flags & IF_KEYFRAME) ? 1 : 0;

	//printf("Frame: %zu  %i \n", len, idx.keyFrame );

	offsets.push_back( idx );

	if (write_chars_bin(out, "00dc", 4) == -1) {
		(void)fprintf(stderr, "gwavi_add_frame: write_chars_bin() "
			      "failed\n");
		return -1;
	}
	if (write_int(out, (unsigned int)(len)) == -1) {
		(void)fprintf(stderr, "gwavi_add_frame: write_int() failed\n");
		return -1;
	}

	if ((t = fwrite(buffer, 1, len, out)) != len) {
		(void)fprintf(stderr, "gwavi_add_frame: fwrite() failed\n");
		return -1;
	}

	for (t = 0; t < maxi_pad; t++)
	{
		if (fputc(0, out) == EOF) {
			(void)fprintf(stderr, "gwavi_add_frame: fputc() failed\n");
			return -1;
		}
	}

	return 0;
}

/**
 * This function allows you to add the audio track to your AVI file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param buffer Audio buffer size.
 * @param len Audio buffer length.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::add_audio( unsigned char *buffer, size_t len)
{
	long long fpos;
	size_t t, maxi_pad;  /* in case audio bleeds over the 4 byte boundary  */
	gwavi_index_rec_t idx;

	if ( !buffer)
	{
		(void)fputs("gwavi and/or buffer argument cannot be NULL",
			    stderr);
		return -1;
	}
	fpos = ftell(out);

	if ( std_index_base_ofs_a == 0 )
	{
		std_index_base_ofs_a = fpos;
	}

	maxi_pad = len % WORD_SIZE;
	if (maxi_pad > 0)
	{
		maxi_pad = WORD_SIZE - maxi_pad;
	}

	idx.fofs     = ftell(out);
	idx.len      = len;
	idx.type     = 1;
	idx.keyFrame = 1;

	offsets.push_back( idx );

	if (write_chars_bin(out,"01wb",4) == -1)
	{
		(void)fprintf(stderr, "gwavi_add_audio: write_chars_bin() "
			      "failed\n");
		return -1;
	}
	if (write_int(out,(unsigned int)(len)) == -1)
	{
		(void)fprintf(stderr, "gwavi_add_audio: write_int() failed\n");
		return -1;
	}

	if ((t = fwrite(buffer, 1, len, out)) != len )
	{
		(void)fprintf(stderr, "gwavi_add_audio: fwrite() failed\n");
		return -1;
	}

	for (t = 0; t < maxi_pad; t++)
	{
		if (fputc(0,out) == EOF)
		{
			(void)fprintf(stderr, "gwavi_add_audio: fputc() failed\n");
			return -1;
		}
	}

	stream_header_a.data_length += (unsigned int)(len + maxi_pad);

	return 0;
}

/**
 * This function should be called when the program is done adding video and/or
 * audio frames to the AVI file. It frees memory allocated for gwavi_open() for
 * the main gwavi_t structure. It also properly closes the output file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::close(void)
{
	long t;

	if ((t = ftell(out)) == -1)
		goto ftell_failed;
	if (fseek(out, marker, SEEK_SET) == -1)
		goto fseek_failed;
	if (write_int(out, (unsigned int)(t - marker - 4)) == -1)
	{
		(void)fprintf(stderr, "gwavi_close: write_int() failed\n");
		return -1;
	}
	if (fseek(out,t,SEEK_SET) == -1)
		goto fseek_failed;

	if ( avi_std < 2 )
	{
		if (write_index1(out) == -1)
		{
			(void)fprintf(stderr, "gwavi_close: write_index() failed\n");
			return -1;
		}
	}
	else
	{
		if ( write_stream_std_indx( out, &stream_index_v ) == -1 )
		{
			return -1;
		}
		if ( audioEnabled )
		{
			if ( write_stream_std_indx( out, &stream_index_a ) == -1 )
			{
				return -1;
			}
		}
	}

	offsets.clear();

	std_index_base_ofs_v = 0;
	std_index_base_ofs_a = 0;

	/* reset some avi header fields */
	avi_header.number_of_frames = stream_header_v.data_length;

	if ((t = ftell(out)) == -1)
		goto ftell_failed;
	if (fseek(out, 12, SEEK_SET) == -1)
		goto fseek_failed;
	if (write_avi_header_chunk(out) == -1) {
		(void)fprintf(stderr, "gwavi_close: write_avi_header_chunk() "
			      "failed\n");
		return -1;
	}
	if (fseek(out, t, SEEK_SET) == -1)
		goto fseek_failed;

	if ((t = ftell(out)) == -1)
		goto ftell_failed;
	if (fseek(out, 4, SEEK_SET) == -1)
		goto fseek_failed;
	if (write_int(out, (unsigned int)(t - 8)) == -1)
	{
		(void)fprintf(stderr, "gwavi_close: write_int() failed\n");
		return -1;
	}
	if (fseek(out, t, SEEK_SET) == -1)
		goto fseek_failed;

	if (stream_format_v.palette != 0)
		free(stream_format_v.palette);

	if (fclose(out) == EOF) {
		perror("gwavi_close (fclose)");
		return -1;
	}
	out = NULL;

	return 0;

ftell_failed:
	perror("gwavi_close: (ftell)");
	return -1;

fseek_failed:
	perror("gwavi_close (fseek)");
	return -1;
}

/**
 * This function allows you to reset the framerate. In a standard use case, you
 * should not need to call it. However, if you need to, you can call it to reset
 * the framerate after you are done adding frames to your AVI file and before
 * you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param fps Number of frames per second of your video.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::set_framerate(double fps)
{
	unsigned int usec;

	usec = (unsigned int)((1000000.0 / fps)+0.50);

	stream_header_v.time_scale = usec;
	stream_header_v.data_rate = 1000000;
	avi_header.time_delay = usec;

	return 0;
}

/**
 * This function allows you to reset the video codec. In a standard use case,
 * you should not need to call it. However, if you need to, you can call it to
 * reset the video codec after you are done adding frames to your AVI file and
 * before you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param fourcc FourCC representing the codec of the video encoded stream. a
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::set_codec( const char *fourcc)
{
	if (check_fourcc(fourcc) != 0)
	{
		(void)fprintf(stderr, "WARNING: given fourcc does not seem to "
			      "be valid: %s\n", fourcc);
	}
	memset( this->fourcc, 0, sizeof(this->fourcc) );
	strcpy( this->fourcc, fourcc );

	memcpy(stream_header_v.codec, fourcc, 4);
	stream_format_v.compression_type =
		((unsigned int)fourcc[3] << 24) +
		((unsigned int)fourcc[2] << 16) +
		((unsigned int)fourcc[1] << 8) +
		((unsigned int)fourcc[0]);

	return 0;
}

/**
 * This function allows you to reset the video size. In a standard use case, you
 * should not need to call it. However, if you need to, you can call it to reset
 * the video height and width set in the AVI file after you are done adding
 * frames to your AVI file and before you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param width Width of a frame.
 * @param height Height of a frame.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::set_size( unsigned int width, unsigned int height)
{
	unsigned int size = (width * height * bits_per_pixel) / 8;

	avi_header.data_rate = size;
	avi_header.width = width;
	avi_header.height = height;
	avi_header.buffer_size = size;
	stream_header_v.buffer_size = size;
	stream_format_v.width = width;
	stream_format_v.height = height;
	stream_format_v.image_size = size;

	return 0;
}

int gwavi_t::printHeaders(void)
{
	char fourcc[8];
	unsigned int ret, fileSize, size;

	if ( in == NULL )
	{
		return -1;
	}

	if (read_chars_bin(in, fourcc, 4) == -1)
		return -1;

	fourcc[4] = 0;
	printf("RIFF Begin: '%s'\n", fourcc );

	if (read_uint(in, fileSize) == -1)
	{
		(void)fprintf(stderr, "gwavi_info: read_int() failed\n");
		return -1;
	}
	size = fileSize;
	printf("FileSize: %u\n", fileSize );

	if (read_chars_bin(in, fourcc, 4) == -1)
		return -1;

	size -= 4;
	fourcc[4] = 0;
	printf("FileType: '%s'\n", fourcc );

	while ( size >= 4 )
	{
		if (read_chars_bin(in, fourcc, 4) == -1)
			return -1;

		fourcc[4] = 0;
		printf("Block: '%s'  %u  0x%X\n", fourcc, size, size );

		size -= 4;

		if ( strcmp( fourcc, "LIST") == 0 )
		{
			ret = readList(1);

			if ( ret == 0 )
			{
				return -1;
			}
			size      -= ret;
		}
		else
		{
			ret = readChunk( fourcc, 1 );

			if ( ret == 0 )
			{
				return -1;
			}
			size      -= ret;
		}
	}

	return 0;
}

unsigned int gwavi_t::readList(int lvl)
{
	unsigned int ret=0, bytesRead=0;
	char fourcc[8], listType[8], pad[4];
	unsigned int size, listSize=0;
	char indent[256];

	memset( indent, ' ', lvl*3);
	indent[lvl*3] = 0;

	if (read_uint(in, listSize) == -1)
	{
		(void)fprintf(stderr, "readList: read_int() failed\n");
		return 0;
	}
	size = listSize;

	if (read_chars_bin(in, listType, 4) == -1)
	{
		return 0;
	}

	listType[4] = 0;

	if ( strcmp( listType, "movi") == 0 )
	{
		movi_fpos = ftell(in) - 4;
	}

	size -= 4;
	bytesRead += 4;

	printf("%sList Start: '%s'  %u\n", indent, listType, listSize );

	while ( size >= 4 )
	{
		if (read_chars_bin(in, fourcc, 4) == -1)
			return 0;

		size -= 4;
		bytesRead += 4;

		fourcc[4] = 0;
		printf("%sBlock: '%s  %u'  0x%X\n", indent, fourcc, size, size );

		if ( strcmp( fourcc, "LIST") == 0 )
		{
			ret = readList(lvl+1);

			if ( ret == 0 )
			{
				return 0;
			}
			size      -= ret;
			bytesRead += ret;
		}
		else
		{
			ret = readChunk( fourcc, lvl+1 );

			if ( ret == 0 )
			{
				return 0;
			}
			size      -= ret;
			bytesRead += ret;
		}
	}

	if ( size > 0 )
	{
		int r = size % WORD_SIZE;

		if (read_chars_bin(in, pad, r) == -1)
		{
			(void)fprintf(stderr, "readList: read_int() failed\n");
			return 0;
		}
		size -= r;
		bytesRead += r;
	}
	printf("%sList End: %s   %u\n", indent, listType, bytesRead);

	return bytesRead+4;
}

unsigned int gwavi_t::readChunk(const char *id, int lvl)
{
	unsigned int r, ret, size, chunkSize, bytesRead=0;
	unsigned short dataWord;
	char indent[256];

	memset( indent, ' ', lvl*3);
	indent[lvl*3] = 0;

	if (read_uint(in, chunkSize) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}
	printf("%sChunk Start: %s   %u\n", indent, id, chunkSize);

	if ( chunkSize == 0 )
	{
		return 0;
	}
	size = chunkSize;

	r = size % WORD_SIZE;

	if ( r > 0 )
	{
		size += r;
	}

	if ( strcmp( id, "avih") == 0 )
	{
		ret = readAviHeader();

		if ( ret == 0 )
		{
			return 0;
		}
		size -= ret;
		bytesRead += ret;
	}
	else if ( strcmp( id, "strh") == 0 )
	{
		ret = readStreamHeader();

		if ( ret == 0 )
		{
			return 0;
		}
		size -= ret;
		bytesRead += ret;
	}
	else if ( strcmp( id, "idx1") == 0 )
	{
		ret = readIndexBlock( chunkSize );

		if ( ret == 0 )
		{
			return 0;
		}
		size -= ret;
		bytesRead += ret;
	}

	while ( size >= WORD_SIZE )
	{
		if (read_ushort(in, dataWord) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_int() failed\n");
			return 0;
		}
		size -= WORD_SIZE;
		bytesRead += WORD_SIZE;
	}

	if ( size > 0 )
	{
		char pad[4];
		int r = size % WORD_SIZE;

		if (read_chars_bin(in, pad, r) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_int() failed\n");
			return 0;
		}
		size -= r;
		bytesRead += r;
	}

	printf("%sChunk End: %s   %u\n", indent, id, bytesRead);

	return bytesRead+4;
}

unsigned int gwavi_t::readAviHeader(void)
{
	gwavi_header_t hdr;

	printf("HDR Size: '%zi'\n", sizeof(hdr) );

	if (read_uint(in, hdr.time_delay) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.data_rate) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.reserved) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.flags) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.number_of_frames) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.initial_frames) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.data_streams) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.buffer_size) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.width) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.height) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.time_scale) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.playback_data_rate) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.starting_time) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.data_length) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	printf("dwMicroSecPerFrame    : '%u'\n", hdr.time_delay );
	printf("dwMaxBytesPerSec      : '%u'\n", hdr.data_rate );
	printf("dwPaddingGranularity  : '%u'\n", hdr.reserved );
	printf("dwFlags               : '%u'\n", hdr.flags );
	printf("dwTotalFrames         : '%u'\n", hdr.number_of_frames );
	printf("dwInitialFrames       : '%u'\n", hdr.initial_frames   );
	printf("dwStreams             : '%u'\n", hdr.data_streams );
	printf("dwSuggestedBufferSize : '%u'\n", hdr.buffer_size );
	printf("dwWidth               : '%u'\n", hdr.width  );
	printf("dwHeight              : '%u'\n", hdr.height );

	return sizeof(gwavi_header_t);
}

unsigned int gwavi_t::readStreamHeader(void)
{
	gwavi_AVIStreamHeader hdr;
	
	printf("HDR Size: '%zi'\n", sizeof(hdr) );

	if (read_chars_bin(in, hdr.fccType, 4) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_chars_bin() failed\n");
		return 0;
	}

	if (read_chars_bin(in, hdr.fccHandler, 4) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_chars_bin() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwFlags) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_ushort(in, hdr.wPriority) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return 0;
	}

	if (read_ushort(in, hdr.wLanguage) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwInitialFrames) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwScale) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwRate) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwStart) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwLength) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwSuggestedBufferSize) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwQuality) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_uint(in, hdr.dwSampleSize) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return 0;
	}

	if (read_short(in, hdr.rcFrame.left) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return 0;
	}

	if (read_short(in, hdr.rcFrame.top) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return 0;
	}

	if (read_short(in, hdr.rcFrame.right) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return 0;
	}

	if (read_short(in, hdr.rcFrame.bottom) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return 0;
	}

	printf("fccType   : '%c%c%c%c'\n",
			hdr.fccType[0], hdr.fccType[1],
				hdr.fccType[2], hdr.fccType[3] );
	printf("fccHandler: '%c%c%c%c'\n",
			hdr.fccHandler[0], hdr.fccHandler[1],
				hdr.fccHandler[2], hdr.fccHandler[3] );
	printf("dwFlags              : '%u'\n", hdr.dwFlags );
	printf("wPriority            : '%u'\n", hdr.wPriority );
	printf("wLanguage            : '%u'\n", hdr.wLanguage );
	printf("dwInitialFrames      : '%u'\n", hdr.dwInitialFrames );
	printf("dwScale              : '%u'\n", hdr.dwScale );
	printf("dwRate               : '%u'\n", hdr.dwRate  );
	printf("dwStart              : '%u'\n", hdr.dwStart );
	printf("dwLength             : '%u'\n", hdr.dwLength );
	printf("dwSuggestedBufferSize: '%u'\n", hdr.dwSuggestedBufferSize );
	printf("dwQuality            : '%u'\n", hdr.dwQuality );
	printf("dwSampleSize         : '%u'\n", hdr.dwSampleSize );
	printf("rcFrame.left         : '%i'\n", hdr.rcFrame.left   );
	printf("rcFrame.top          : '%i'\n", hdr.rcFrame.top    );
	printf("rcFrame.right        : '%i'\n", hdr.rcFrame.right  );
	printf("rcFrame.bottom       : '%i'\n", hdr.rcFrame.bottom );

	return sizeof(gwavi_AVIStreamHeader);
}

unsigned int gwavi_t::readIndexBlock( unsigned int chunkSize )
{
	char chunkId[8];
	unsigned int size, flags, ofs, ckSize, bytesRead=0;

	size = chunkSize;

	while ( size >= 4 )
	{
		if (read_chars_bin(in, chunkId, 4) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_chars_bin() failed\n");
			return 0;
		}
		chunkId[4] = 0;

		if (read_uint(in, flags) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_uint() failed\n");
			return 0;
		}

		if (read_uint(in, ofs) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_uint() failed\n");
			return 0;
		}

		if (read_uint(in, ckSize) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_uint() failed\n");
			return 0;
		}

		printf("     Index: %s  0x%X  ofs:%u  size:%u\n", chunkId, flags, ofs, ckSize );

		peak_chunk( in, ofs, chunkId, &ckSize );

		printf("Peak Index: %s  0x%X  ofs:%u  size:%u\n", chunkId, flags, ofs, ckSize );

		size      -= 16;
		bytesRead += 16;
	}
	return bytesRead;
}
