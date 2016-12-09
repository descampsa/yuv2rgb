#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/opt.h>

#include "../yuv_rgb.h"

/*
Compile(OS X):
	cc -I /usr/local/Cellar/ffmpeg/3.0.1/include -Wall -g -c \
			-o bench.o bench.c
	cc  bench.o -L /usr/local/Cellar/ffmpeg/3.0.1/lib \
			-lavdevice -lavformat -lavfilter -lavcodec \
			-lswresample -lswscale -lavutil  \
			-o bench

AV_PIX_FMT_RGB24  : 2
AV_PIX_FMT_YUV420P: 0

*/

char* guess_image_ext(const char* img_file_name) {
	char _img_file_name[strlen(img_file_name)];
	char *delim = ".";
	char* p;
	char* ext="";

	strtok(strcpy(_img_file_name, img_file_name), delim);
	while((p = strtok(NULL, delim))){
		ext = p;
	}
	if ( !strcmp(ext, "png") ) {
		return "png";
	} else if ( !strcmp(ext, "jpg") ) {
		return "jpg";
	} else if ( !strcmp(ext, "yuv") ) {
		return "yuv";
	} else {
		return "";
	}
}

int ffmpeg_image_to_avframe(const char* img_file_name, AVFrame* frame){
	AVFormatContext *fmt_ctx = avformat_alloc_context();
	// int width=1084, height=2318; // lenna : 1084x2318
	int width=1920, height=1080;
	int ret=0;
	
	int frameFinished, numBytes;
	AVPacket packet;

	char* image_ext = guess_image_ext(img_file_name);

	if ( strlen(image_ext) == 0 ) {
		printf("Image format not support. \n");
		return 1;
	}

	if ( !strcmp(image_ext, "png") || !strcmp(image_ext, "jpg") ) {
		if ( avformat_open_input(&fmt_ctx, img_file_name, NULL, NULL) != 0 ) {
			printf("Can't open image file '%s'\n", img_file_name);
			return 2;
		}

		AVCodecContext *codec_ctx;
		codec_ctx          = fmt_ctx->streams[0]->codec;
		codec_ctx->width   = width;
		codec_ctx->height  = height;
		codec_ctx->pix_fmt = AV_PIX_FMT_RGB24; // AV_PIX_FMT_YUV420P

		numBytes = width*height*3;
		AVCodec *codec = avcodec_find_decoder(codec_ctx->codec_id);

		if (!codec) {
			printf("Codec not found\n");
			return 3;
		}
		if(avcodec_open2(codec_ctx, codec, NULL)<0) {
			printf("Could not open codec\n");
			return 4;
		}
		if (!frame) {
			printf("Can't allocate memory for AVFrame\n");
			return 5;
		}

		uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
		av_image_fill_arrays(frame->data, frame->linesize, buffer,
							 codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, 1);

		while (av_read_frame(fmt_ctx, &packet) >= 0) {
			if(packet.stream_index != 0)
				continue;
			int size = avcodec_decode_video2(codec_ctx, frame, &frameFinished, &packet);
			if (size > 0) {
				frame->quality = 7;
			} else {
				printf("Error [%d] while decoding frame: %s\n", size, strerror(AVERROR(size)));
				return 6;
			}
		}
	} else if ( !strcmp(image_ext, "yuv") ){
		numBytes = width*height*1.5;
		// Read Raw YUV420P File
		FILE *file = fopen(img_file_name, "rb");
		if(!file){
			perror("Error opening yuv image for read");
			return 7;
		}
		// fseek(file, 0, SEEK_END);
		// uint32_t size = ftell(file);
		// if( size!= (uint32_t)numBytes ) {
		//     fprintf(stderr, "Wrong size of yuv image.\n");
		//     fclose(file);
		//     return 8;
		// }
		// fseek(file, 0, SEEK_SET);
		// AVFrame *dst_frame = av_frame_alloc();
		frame->width  = width;
		frame->height = height;
		frame->format = AV_PIX_FMT_YUV420P;
		
		ret = av_image_alloc(frame->data, frame->linesize, 
							 frame->width, frame->height, 
							 frame->format, 1);
		if ( ret < 0 ) {
			// FFMPEG Error.
			return 1;
		}
		// uint8_t *YUV=NULL;
		// *YUV = malloc(size);
		// size_t result = fread(*YUV, 1, size, file);
		// if (result != size) {
		//     perror("Error reading yuv image");
		//     fclose(file);
		//     return 9;
		// }

		int yuv_size  = frame->width*frame->height*3/2;
		int ysize     = frame->width*frame->height;
		int uvsize    = frame->width*frame->height/4;

		// uint8_t* buf = (uint8_t *)av_malloc(yuv_size);
		// ret = fread(buf, 1, yuv_size, file);
		// if ( ret <= 0 ) {
		//     printf("Could not read yuv image file.");
		//     return 8;
		// }
		// frame->data[0] = buf;            // Y
		// frame->data[1] = buf + ysize;    // U 
		// frame->data[2] = buf + ysize*5/4;// V

		fread(frame->data[0], 1, ysize,  file); // Y
		fread(frame->data[1], 1, uvsize, file); // U
		fread(frame->data[2], 1, uvsize, file); // V
		fclose(file);
		return 0;
	}

	return 0;
}

int ffmpeg_sws_scale(AVFrame* src_frame, AVFrame* dst_frame) {
	struct SwsContext *sws_ctx;

	sws_ctx = sws_getContext(src_frame->width, src_frame->height, src_frame->format, 
							 dst_frame->width, dst_frame->height, dst_frame->format,
							 SWS_BICUBIC, NULL, NULL, NULL);
	sws_scale(sws_ctx, (const uint8_t *const *)src_frame->data, src_frame->linesize,
			  0, src_frame->height, dst_frame->data, dst_frame->linesize);

	sws_freeContext(sws_ctx);

	// av_freep(&src_data[0]);
	return 0;
}

int ffmpeg_save_avframe(AVFrame *frame, int frame_no) {
	AVCodecContext *codec_context;
	AVCodec        *codec;
	AVPacket       packet;

	int  gotFrame;
	char filename[256];
	FILE *file;
	
	if ( frame->format == AV_PIX_FMT_RGB24 ) {
		sprintf(filename, "ffmpeg_save_%06d_%dx%d.jpg", frame_no, frame->width, frame->height);
		file = fopen(filename, "wb");
		codec = avcodec_find_encoder(AV_CODEC_ID_JPEG2000);
	} else if ( frame->format == AV_PIX_FMT_YUV420P ) {
		// Write Raw YUV420P File
		sprintf(filename, "ffmpeg_save_%06d_%dx%d.yuv", frame_no, frame->width, frame->height);
		file = fopen(filename, "wb");
		fwrite(frame->data[0], 1, frame->width*frame->height,   file); // Y
		fwrite(frame->data[1], 1, frame->width*frame->height/4, file); // U
		fwrite(frame->data[2], 1, frame->width*frame->height/4, file); // V
		fclose(file);
		printf("[Done] File name: %s\n", filename);
		printf("[Play] ffplay -f rawvideo -pixel_format yuv420p -video_size %dx%d %s\n", 
				frame->width, frame->height, filename);
		return 0;
	} else {
		// Not Support PixelFormat
		printf("[Error] PixelFormat(%d) not support.\n", frame->format);
		return 1;
	}

	if (!codec) {
		return 1;
	}
	codec_context = avcodec_alloc_context3(codec);
	if (!codec_context) {
		return 2;
	}

	// codec_context->pix_fmt = AV_PIX_FMT_RGB24;
	codec_context->pix_fmt = frame->format;
	codec_context->height  = frame->height;
	codec_context->width   = frame->width;

	if (avcodec_open2(codec_context, codec, NULL) < 0) {
		return 3;
	}
	
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;

	if (avcodec_encode_video2(codec_context, &packet, frame, &gotFrame) < 0) {
		return 4;
	}

	fwrite(packet.data, 1, packet.size, file);
	printf("[Done] File name: %s\n", filename);
	fclose(file);

	// warning: 'av_free_packet' is deprecated
	// av_free_packet(&packet);
	av_freep(&packet);
	avcodec_close(codec_context);
	return 0;
}

int ffmpeg_yuv420p_to_rgb24(const char* img_file_name){
	AVFrame *src_frame = av_frame_alloc();
	AVFrame *dst_frame = av_frame_alloc();
	int ret=0;

	ret = ffmpeg_image_to_avframe(img_file_name, src_frame);
	if ( ret > 0 ) {
		printf("[Error] ffmpeg_image_to_avframe error. \n");
		return 2;
	}

	dst_frame->width  = src_frame->width;
	dst_frame->height = src_frame->height;
	dst_frame->format = AV_PIX_FMT_RGB24;
	
	ret = av_image_alloc(dst_frame->data, dst_frame->linesize, 
						 dst_frame->width, dst_frame->height, 
						 dst_frame->format, 1);
	if ( ret < 0 ) {
		// FFMPEG Error.
		return 1;
	}

	ret = ffmpeg_sws_scale(src_frame, dst_frame);
	if ( ret > 0 ) {
		printf("[Error] ffmpeg_sws_scale error. \n");
		return 2;
	}

	ffmpeg_save_avframe(dst_frame, 1);

	av_freep(src_frame);
	av_freep(dst_frame);

	return 0;
}

int ffmpeg_rgb24_to_yuv420p(const char* img_file_name){
	AVFrame *src_frame = av_frame_alloc();
	AVFrame *dst_frame = av_frame_alloc();
	int ret=0;

	ret = ffmpeg_image_to_avframe(img_file_name, src_frame);
	if ( ret > 0 ) {
		printf("[Error] ffmpeg_image_to_avframe error. \n");
		return 2;
	}

	dst_frame->width  = src_frame->width;
	dst_frame->height = src_frame->height;
	dst_frame->format = AV_PIX_FMT_YUV420P;
	
	ret = av_image_alloc(dst_frame->data, dst_frame->linesize, 
						 dst_frame->width, dst_frame->height, 
						 dst_frame->format, 1);
	if ( ret < 0 ) {
		// FFMPEG Error.
		return 1;
	}

	ret = ffmpeg_sws_scale(src_frame, dst_frame);
	if ( ret > 0 ) {
		printf("[Error] ffmpeg_sws_scale error. \n");
		return 2;
	}

	ffmpeg_save_avframe(dst_frame, 1);

	av_freep(src_frame);
	av_freep(dst_frame);
	return 0;
}

int std_yuv420p_to_rgb24(const char* img_file_name){
	printf("%s\n", img_file_name);
	return 0;
}

int std_rgb24_to_yuv420p(const char* img_file_name){
	
	AVFrame *src_frame = av_frame_alloc();
	AVFrame *dst_frame = av_frame_alloc();
	int ret=0;

	ret = ffmpeg_image_to_avframe(img_file_name, src_frame);
	if ( ret > 0 ) {
		printf("[Error] ffmpeg_image_to_avframe error. \n");
		return 2;
	}
	dst_frame->width  = src_frame->width;
	dst_frame->height = src_frame->height;
	dst_frame->format = AV_PIX_FMT_YUV420P;
	
	ret = av_image_alloc(dst_frame->data, dst_frame->linesize, 
						 dst_frame->width, dst_frame->height, 
						 dst_frame->format, 1);
	if ( ret < 0 ) {
		// FFMPEG Error.
		return 1;
	}

	// const size_t y_stride   = width + (16-width%16)%16,
	//              uv_stride  = (width+1)/2 + (16-((width+1)/2)%16)%16,
	//              rgb_stride = width*3 +(16-(3*width)%16)%16;
	// const YCbCrType yuv_format = YCBCR_601;
	// rgb24_yuv420_std(width, height, rgb, rgb_stride, y, u, v, y_stride, uv_stride, yuv_type);
	rgb24_yuv420_std(src_frame->width, src_frame->height, src_frame->data[0],
					 src_frame->linesize[0],
					 dst_frame->data[0], dst_frame->data[1], dst_frame->data[2],
					 dst_frame->linesize[0], dst_frame->linesize[1], YCBCR_601);

	ffmpeg_save_avframe(dst_frame, 2);

	av_freep(src_frame);
	av_freep(dst_frame);
	return 0;
}

int ipp_yuv420p_to_rgb24(const char* img_file_name){
	printf("%s\n", img_file_name);
	return 0;
}

int ipp_rgb24_to_yuv420p(const char* img_file_name){
	printf("%s\n", img_file_name);
	return 0;
}

void ffmpeg_bench(){
	printf("[Bench] ffmpeg bench ... \n");
	av_register_all();

	// ensure image resolution is 1920x1080
	printf("\t ffmpeg rgb24 to yuv420p \n");
	ffmpeg_rgb24_to_yuv420p("lenna.jpg");
	

	printf("\t ffmpeg yuv420p to rgb24 \n");
	ffmpeg_yuv420p_to_rgb24("lenna.yuv");
}

void std_bench() {
	printf("[Bench] std bench ... \n");
	av_register_all();

	// ensure image resolution is 1920x1080
	printf("\t std rgb24 to yuv420p \n");
	std_rgb24_to_yuv420p("lenna.jpg");
}

void ipp_bench(){
	printf("[Bench] IPP bench ... \n");
	printf("\t *TODO* \n");
	// uint8_t *RGB=NULL;
	// const Ipp8u* pSrc = RGB;
	// int srcStep = rgb_stride;

	// Ipp8u* pDst[3] = {y, u, v};
	// int dstStep[3] = {y_stride, uv_stride, uv_stride};
	// IppiSize imgSize = {.width=width, .height=height};

	// // ippiRGBToYCbCr420_8u_C3P3R(pSrc, srcStep, pDst, dstStep, imgSize);
	// ippiRGBToYCbCr420_8u_C3P3R(pSrc, linesize, pDst, dstStep, imgSize);
}

int main(int argc, char **argv){
	// ensure you already have `lenna.jpg` and `lenna.yuv` image in this dir.
	// or download from:
	//      wget http://wallpapershome.com/images/wallpapers/yosemite-1920x1080-5k-wallpapers-forest-osx-apple-mountains-181.jpg \
	//          -O lenna.jpg
	//      ffmpeg -i lenna.jpg lenna.yuv

	ffmpeg_bench();
	std_bench();
	ipp_bench();
	return 0;
}
