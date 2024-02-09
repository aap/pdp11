#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
#define nil NULL

#include "lodepng.h"

typedef struct Image Image;
struct Image
{
	u32 w, h;
	u32 stride;
	u8 *pixels;
};

Image*
loadpng(u8 *data, u32 len)
{
	u32 w, h, error;
	u8 *pixels;
        LodePNGState state;
	Image *img;

        lodepng_state_init(&state);
	error = lodepng_decode(&pixels, &w, &h, &state, data, len);
	if(error) {
		printf("lodepng error %s\n", lodepng_error_text(error));
		return nil;
	}

	img = malloc(sizeof(Image));
	img->w = w;
	img->stride = w*4;
	img->h = h;
	img->pixels = pixels;
	return img;
}

u8*
loadfile(const char *path, u32 *plen)
{
	FILE *f;
	u8 *data;
	u32 len;

	f = fopen(path, "rb");
	if(f == nil)
		return nil;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc(len);
	fread(data, 1, len, f);
	fclose(f);

	*plen = len;
	return data;
}

void
writeimg(const char *path, Image *img)
{
	FILE *f;
	u8 *raw;
	size_t len;
	u32 error;
        LodePNGState state;

        lodepng_state_init(&state);
	error = lodepng_encode(&raw, &len, img->pixels, img->w, img->h, &state);
	if(error) {
		printf("can't encode png\n");
		return;
	}

	f = fopen(path, "wb");
	if(f == nil) {
		printf("can't open output\n");
		return;
	}
	fwrite(raw, 1, len, f);
	fclose(f);
}

float
meta(float x, float y, float r, float bx, float by)
{
	float dx = x-bx;
	float dy = y-by;
	return r*r/(dx*dx + dy*dy);
}

float
smoothstep(float e0, float e1, float x)
{
	x = (x-e0)/(e1-e0);
	if(x < 0.0f) x = 0.0f;
	if(x > 1.0f) x = 1.0f;
	return x*x*(3.0f - 2.0f*x);
}

void
metaballize(Image *in, Image *out, int mag)
{
	u32 x, y;
	u8 *p;

//	int offx = 192;
//	int offy = 68;
	int offx = 0;
	int offy = 0;
	int sz = 3;

	for(y = 0; y < out->h; y++)
		for(x = 0; x < out->w; x++) {
			p = &out->pixels[y*out->stride + x*4];

			float fx = offx + (float)x/mag;
			float fy = offy + (float)y/mag;

			float f = 0.0f;
			for(int i = (int)fy - sz; i < (int)fy + sz; i++)
			for(int j = (int)fx - sz; j < (int)fx + sz; j++)
				if(i >= 0 && i < in->h &&
				   j >= 0 && j < in->w &&
				   in->pixels[i*in->stride + j] == 0) {
					float m = meta(fx,fy, 1.00f, j, i);
					m = powf(m, 2.2f);
					f += m;
				}

#if 0
			int c = 255;
			if(f > 1.0f) {
				c = 0;
			}
#else
			int c = smoothstep(1.0f, 0.7f, f)*255;
#endif
			p[0] = c;
			p[1] = c;
			p[2] = c;
			p[3] = 255;
		}
}

void
downsample(Image *out, Image *in, int mag)
{
	u32 x, y;
	u8 *p;

	assert(out->w*4 == in->w);
	assert(out->h*4 == in->h);

	for(y = 0; y < out->h; y++)
		for(x = 0; x < out->w; x++) {
			p = &out->pixels[y*out->stride + x*4];

			int c = 0;
			for(int i = 0; i < mag; i++)
			for(int j = 0; j < mag; j++)
				c += in->pixels[(y*mag+i)*in->stride + (x*mag+j)*4];
			c /= (mag*mag);

			p[0] = c;
			p[1] = c;
			p[2] = c;
			p[3] = 255;
		}
}

void meatball(int width, int height, unsigned char *paper, unsigned char *image, unsigned char *page)
{
	Image in, meatball, out;
	int mag = 4;

	in.w = width;
	in.h = height;
	in.stride = in.w;
	in.pixels = paper;

	meatball.w = mag*width;
	meatball.h = mag*height;
	meatball.stride = 4*meatball.w;
	meatball.pixels = image;

	out.w = width;
	out.h = height;
	out.stride = 4*in.w;
	out.pixels = page;

	metaballize(&in, &meatball, mag);
	downsample(&out, &meatball, mag);
}

int
omain(int argc, char *argv[])
{
	FILE *f;
	u8 *raw;
	u32 len;
	Image *img, *out;

	if(argc < 2) {
		fprintf(stderr, "usage: %s input.png [output.png]\n", argv[0]);
		return 1;
	}
	raw = loadfile(argv[1], &len);
	if(raw == nil) {
		printf("can't open input\n");
		return 1;
	}
	img = loadpng(raw, len);
	if(img == nil) {
		printf("can't read png\n");
		return 1;
	}
	free(raw);

	int mag = 4;

	out = malloc(sizeof(Image));
	out->w = img->w*mag;
	out->h = img->h*mag;
	out->stride = out->w*4;
	out->pixels = malloc(out->w*out->h*4);
	memset(out->pixels, 0, out->w*out->h*4);

	metaballize(img, out, mag);
//	writeimg("out.png", out);

	downsample(img, out, mag);
	if(argc > 2)
		writeimg(argv[2], img);
	else
		writeimg("out.png", img);

	return 0;
}
