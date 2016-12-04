// Copyright 2016 Adrien Descamps
// Distributed under BSD 3-Clause License

#include "yuv_rgb.h"

#include <x86intrin.h>

#include <stdio.h>

#define PRECISION 6
#define PRECISION_FACTOR (1<<PRECISION)

typedef struct
{
	uint8_t y_shift;
	int16_t matrix[3][3];
} RGB2YUVParam;
// |Y|   |y_shift|                        |matrix[0][0] matrix[0][1] matrix[0][2]|   |R|
// |U| = |  128  | + 1/PRECISION_FACTOR * |matrix[1][0] matrix[1][1] matrix[1][2]| * |G|
// |V|   |  128  |                        |matrix[2][0] matrix[2][1] matrix[2][2]|   |B|

typedef struct
{
	uint8_t y_shift;
	int16_t y_factor;
	int16_t v_r_factor;
	int16_t u_g_factor;
	int16_t v_g_factor;
	int16_t u_b_factor;
} YUV2RGBParam;
// |R|                        |y_factor      0       v_r_factor|   |Y-y_shift|
// |G| = 1/PRECISION_FACTOR * |y_factor  u_g_factor  v_g_factor| * |  U-128  |
// |B|                        |y_factor  u_b_factor      0     |   |  V-128  |

#define V(value) (value*PRECISION_FACTOR)

// for ITU-T T.871, values can be found in section 7
// for ITU-R BT.601-7 values are derived from equations in sections 2.5.1-2.5.3, assuming RGB is encoded using full range ([0-1]<->[0-255])
// for ITU-R BT.709-6 values are derived from equations in sections 3.2-3.4, assuming RGB is encoded using full range ([0-1]<->[0-255])
// all values are rounded to the fourth decimal

static const YUV2RGBParam YUV2RGB[3] = {
	// ITU-T T.871 (JPEG)
	{.y_shift=0, .y_factor=V(1.0), .v_r_factor=V(1.402), .u_g_factor=V(-0.3441), .v_g_factor=V(-0.7141), .u_b_factor=V(1.772)},
	// ITU-R BT.601-7
	{.y_shift=16, .y_factor=V(1.1644), .v_r_factor=V(1.596), .u_g_factor=V(-0.3918), .v_g_factor=V(-0.813), .u_b_factor=V(2.0172)},
	// ITU-R BT.709-6
	{.y_shift=16, .y_factor=V(1.1644), .v_r_factor=V(1.7927), .u_g_factor=V(-0.2132), .v_g_factor=V(-0.5329), .u_b_factor=V(2.1124)}
};

static const RGB2YUVParam RGB2YUV[3] = {
	// ITU-T T.871 (JPEG)
	{.y_shift=0, .matrix={{V(0.299), V(0.587), V(0.114)}, {V(-0.1687), V(-0.3313), V(0.5)}, {V(0.5), V(-0.4187), V(-0.0813)}}},
	// ITU-R BT.601-7
	{.y_shift=16, .matrix={{V(0.2568), V(0.5041), V(0.0979)}, {V(-0.1482), V(-0.291), V(0.4392)}, {V(0.4392), V(-0.3678), V(-0.0714)}}},
	// ITU-R BT.709-6
	{.y_shift=16, .matrix={{V(0.1826), V(0.6142), V(0.062)}, {V(-0.1006), V(-0.3386), V(0.4392)}, {V(0.4392), V(-0.3989), V(-0.0403)}}}
};

// divide by PRECISION_FACTOR and clamp to [0:255] interval
// input must be in the [-128*PRECISION_FACTOR:384*PRECISION_FACTOR] range
static inline uint8_t clampU8(int32_t v)
{
	static const uint8_t lut[512] = 
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,
	47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,
	91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,
	126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,
	159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
	192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,
	225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
	};
	return lut[(v+128*PRECISION_FACTOR)>>PRECISION];
}

void yuv420_rgb24_std(
	uint32_t width, uint32_t height, 
	const uint8_t *Y, const uint8_t *U, const uint8_t *V, uint32_t Y_stride, uint32_t UV_stride, 
	uint8_t *RGB, uint32_t RGB_stride, 
	YCbCrType yuv_type)
{
	const YUV2RGBParam *const param = &(YUV2RGB[yuv_type]);
	
	uint32_t x, y;
	for(y=0; y<(height-1); y+=2)
	{
		const uint8_t *y_ptr1=Y+y*Y_stride,
			*y_ptr2=Y+(y+1)*Y_stride,
			*u_ptr=U+(y/2)*UV_stride,
			*v_ptr=V+(y/2)*UV_stride;
		
		uint8_t *rgb_ptr1=RGB+y*RGB_stride,
			*rgb_ptr2=RGB+(y+1)*RGB_stride;
		
		for(x=0; x<(width-1); x+=2)
		{
			// Compute U and V contributions, common to the four pixels
			
			int32_t u_tmp = ((*u_ptr)-128);
			int32_t v_tmp = ((*v_ptr)-128);
			
			int32_t r_tmp = (v_tmp*param->v_r_factor);
			int32_t g_tmp = (u_tmp*param->u_g_factor + v_tmp*param->v_g_factor);
			int32_t b_tmp = (u_tmp*param->u_b_factor);
			
			// Compute the Y contribution for each pixel
			
			int32_t y_tmp = ((y_ptr1[0]-param->y_shift)*param->y_factor);
			rgb_ptr1[0] = clampU8(y_tmp+r_tmp);
			rgb_ptr1[1] = clampU8(y_tmp+g_tmp);
			rgb_ptr1[2] = clampU8(y_tmp+b_tmp);
			
			y_tmp = ((y_ptr1[1]-param->y_shift)*param->y_factor);
			rgb_ptr1[3] = clampU8(y_tmp+r_tmp);
			rgb_ptr1[4] = clampU8(y_tmp+g_tmp);
			rgb_ptr1[5] = clampU8(y_tmp+b_tmp);
			
			y_tmp = ((y_ptr2[0]-param->y_shift)*param->y_factor);
			rgb_ptr2[0] = clampU8(y_tmp+r_tmp);
			rgb_ptr2[1] = clampU8(y_tmp+g_tmp);
			rgb_ptr2[2] = clampU8(y_tmp+b_tmp);
			
			y_tmp = ((y_ptr2[1]-param->y_shift)*param->y_factor);
			rgb_ptr2[3] = clampU8(y_tmp+r_tmp);
			rgb_ptr2[4] = clampU8(y_tmp+g_tmp);
			rgb_ptr2[5] = clampU8(y_tmp+b_tmp);
			
			y_ptr1+=2;
			y_ptr2+=2;
			++u_ptr; 
			++v_ptr;
			rgb_ptr1+=6;
			rgb_ptr2+=6;
		}
	}
}

void rgb24_yuv420_std(
	uint32_t width, uint32_t height, 
	const uint8_t *RGB, uint32_t RGB_stride, 
	uint8_t *Y, uint8_t *U, uint8_t *V, uint32_t Y_stride, uint32_t UV_stride, 
	YCbCrType yuv_type)
{
	const RGB2YUVParam *const param = &(RGB2YUV[yuv_type]);
	
	uint32_t x, y;
	for(y=0; y<(height-1); y+=2)
	{
		const uint8_t *rgb_ptr1=RGB+y*RGB_stride,
			*rgb_ptr2=RGB+(y+1)*RGB_stride;
			
		uint8_t *y_ptr1=Y+y*Y_stride,
			*y_ptr2=Y+(y+1)*Y_stride,
			*u_ptr=U+(y/2)*UV_stride,
			*v_ptr=V+(y/2)*UV_stride;
		
		for(x=0; x<(width-1); x+=2)
		{
			// compute yuv for the four pixels, u and v values are summed
			int32_t y_tmp, u_tmp, v_tmp;
			
			y_tmp = param->matrix[0][0]*rgb_ptr1[0] + param->matrix[0][1]*rgb_ptr1[1] + param->matrix[0][2]*rgb_ptr1[2];
			u_tmp = param->matrix[1][0]*rgb_ptr1[0] + param->matrix[1][1]*rgb_ptr1[1] + param->matrix[1][2]*rgb_ptr1[2];
			v_tmp = param->matrix[2][0]*rgb_ptr1[0] + param->matrix[2][1]*rgb_ptr1[1] + param->matrix[2][2]*rgb_ptr1[2];
			y_ptr1[0]=clampU8(y_tmp+((param->y_shift)<<PRECISION));
			
			y_tmp = param->matrix[0][0]*rgb_ptr1[3] + param->matrix[0][1]*rgb_ptr1[4] + param->matrix[0][2]*rgb_ptr1[5];
			u_tmp += param->matrix[1][0]*rgb_ptr1[3] + param->matrix[1][1]*rgb_ptr1[4] + param->matrix[1][2]*rgb_ptr1[5];
			v_tmp += param->matrix[2][0]*rgb_ptr1[3] + param->matrix[2][1]*rgb_ptr1[4] + param->matrix[2][2]*rgb_ptr1[5];
			y_ptr1[1]=clampU8(y_tmp+((param->y_shift)<<PRECISION));
			
			y_tmp = param->matrix[0][0]*rgb_ptr2[0] + param->matrix[0][1]*rgb_ptr2[1] + param->matrix[0][2]*rgb_ptr2[2];
			u_tmp += param->matrix[1][0]*rgb_ptr2[0] + param->matrix[1][1]*rgb_ptr2[1] + param->matrix[1][2]*rgb_ptr2[2];
			v_tmp += param->matrix[2][0]*rgb_ptr2[0] + param->matrix[2][1]*rgb_ptr2[1] + param->matrix[2][2]*rgb_ptr2[2];
			y_ptr2[0]=clampU8(y_tmp+((param->y_shift)<<PRECISION));
			
			y_tmp = param->matrix[0][0]*rgb_ptr2[3] + param->matrix[0][1]*rgb_ptr2[4] + param->matrix[0][2]*rgb_ptr2[5];
			u_tmp += param->matrix[1][0]*rgb_ptr2[3] + param->matrix[1][1]*rgb_ptr2[4] + param->matrix[1][2]*rgb_ptr2[5];
			v_tmp += param->matrix[2][0]*rgb_ptr2[3] + param->matrix[2][1]*rgb_ptr2[4] + param->matrix[2][2]*rgb_ptr2[5];
			y_ptr2[1]=clampU8(y_tmp+((param->y_shift)<<PRECISION));
			
			u_ptr[0] = clampU8(u_tmp/4+(128<<PRECISION));
			v_ptr[0] = clampU8(v_tmp/4+(128<<PRECISION));
			
			rgb_ptr1 += 6;
			rgb_ptr2 += 6;
			y_ptr1 += 2;
			y_ptr2 += 2;
			u_ptr += 1;
			v_ptr += 1;
		}
	}
}

#ifdef __SSE2__

#define UV2RGB_16(U,V,R1,G1,B1,R2,G2,B2) \
	r_tmp = _mm_mullo_epi16(V, _mm_set1_epi16(param->v_r_factor)); \
	g_tmp = _mm_add_epi16( \
		_mm_mullo_epi16(U, _mm_set1_epi16(param->u_g_factor)), \
		_mm_mullo_epi16(V, _mm_set1_epi16(param->v_g_factor))); \
	b_tmp = _mm_mullo_epi16(U, _mm_set1_epi16(param->u_b_factor)); \
	R1 = _mm_unpacklo_epi16(r_tmp, r_tmp); \
	G1 = _mm_unpacklo_epi16(g_tmp, g_tmp); \
	B1 = _mm_unpacklo_epi16(b_tmp, b_tmp); \
	R2 = _mm_unpackhi_epi16(r_tmp, r_tmp); \
	G2 = _mm_unpackhi_epi16(g_tmp, g_tmp); \
	B2 = _mm_unpackhi_epi16(b_tmp, b_tmp); \

#define ADD_Y2RGB_16(Y1,Y2,R1,G1,B1,R2,G2,B2) \
	Y1 = _mm_mullo_epi16(_mm_sub_epi16(Y1, _mm_set1_epi16(param->y_shift)), _mm_set1_epi16(param->y_factor)); \
	Y2 = _mm_mullo_epi16(_mm_sub_epi16(Y2, _mm_set1_epi16(param->y_shift)), _mm_set1_epi16(param->y_factor)); \
	\
	R1 = _mm_srai_epi16(_mm_add_epi16(R1, Y1), PRECISION); \
	G1 = _mm_srai_epi16(_mm_add_epi16(G1, Y1), PRECISION); \
	B1 = _mm_srai_epi16(_mm_add_epi16(B1, Y1), PRECISION); \
	R2 = _mm_srai_epi16(_mm_add_epi16(R2, Y2), PRECISION); \
	G2 = _mm_srai_epi16(_mm_add_epi16(G2, Y2), PRECISION); \
	B2 = _mm_srai_epi16(_mm_add_epi16(B2, Y2), PRECISION); \

#define PACK_RGB24_32_STEP1(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
RGB1 = _mm_packus_epi16(_mm_and_si128(R1,_mm_set1_epi16(0xFF)), _mm_and_si128(R2,_mm_set1_epi16(0xFF))); \
RGB2 = _mm_packus_epi16(_mm_and_si128(G1,_mm_set1_epi16(0xFF)), _mm_and_si128(G2,_mm_set1_epi16(0xFF))); \
RGB3 = _mm_packus_epi16(_mm_and_si128(B1,_mm_set1_epi16(0xFF)), _mm_and_si128(B2,_mm_set1_epi16(0xFF))); \
RGB4 = _mm_packus_epi16(_mm_srli_epi16(R1,8), _mm_srli_epi16(R2,8)); \
RGB5 = _mm_packus_epi16(_mm_srli_epi16(G1,8), _mm_srli_epi16(G2,8)); \
RGB6 = _mm_packus_epi16(_mm_srli_epi16(B1,8), _mm_srli_epi16(B2,8)); \

#define PACK_RGB24_32_STEP2(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
R1 = _mm_packus_epi16(_mm_and_si128(RGB1,_mm_set1_epi16(0xFF)), _mm_and_si128(RGB2,_mm_set1_epi16(0xFF))); \
R2 = _mm_packus_epi16(_mm_and_si128(RGB3,_mm_set1_epi16(0xFF)), _mm_and_si128(RGB4,_mm_set1_epi16(0xFF))); \
G1 = _mm_packus_epi16(_mm_and_si128(RGB5,_mm_set1_epi16(0xFF)), _mm_and_si128(RGB6,_mm_set1_epi16(0xFF))); \
G2 = _mm_packus_epi16(_mm_srli_epi16(RGB1,8), _mm_srli_epi16(RGB2,8)); \
B1 = _mm_packus_epi16(_mm_srli_epi16(RGB3,8), _mm_srli_epi16(RGB4,8)); \
B2 = _mm_packus_epi16(_mm_srli_epi16(RGB5,8), _mm_srli_epi16(RGB6,8)); \

#define PACK_RGB24_32(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP1(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP2(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP1(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP2(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP1(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \

#define YUV2RGB_32 \
	__m128i r_tmp, g_tmp, b_tmp; \
	__m128i r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2; \
	__m128i r_uv_16_1, g_uv_16_1, b_uv_16_1, r_uv_16_2, g_uv_16_2, b_uv_16_2; \
	__m128i y_16_1, y_16_2; \
	\
	__m128i u = LOAD_SI128((const __m128i*)(u_ptr)); \
	__m128i v = LOAD_SI128((const __m128i*)(v_ptr)); \
	\
	/* process first 16 pixels of first line */\
	__m128i u_16 = _mm_unpacklo_epi8(u, _mm_setzero_si128()); \
	__m128i v_16 = _mm_unpacklo_epi8(v, _mm_setzero_si128()); \
	u_16 = _mm_add_epi16(u_16, _mm_set1_epi16(-128)); \
	v_16 = _mm_add_epi16(v_16, _mm_set1_epi16(-128)); \
	\
	UV2RGB_16(u_16, v_16, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2) \
	r_uv_16_1=r_16_1; g_uv_16_1=g_16_1; b_uv_16_1=b_16_1; \
	r_uv_16_2=r_16_2; g_uv_16_2=g_16_2; b_uv_16_2=b_16_2; \
	\
	__m128i y = LOAD_SI128((const __m128i*)(y_ptr1)); \
	y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128()); \
	y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128()); \
	\
	ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2) \
	\
	__m128i r_8_11 = _mm_packus_epi16(r_16_1, r_16_2); \
	__m128i g_8_11 = _mm_packus_epi16(g_16_1, g_16_2); \
	__m128i b_8_11 = _mm_packus_epi16(b_16_1, b_16_2); \
	\
	/* process first 16 pixels of second line */\
	r_16_1=r_uv_16_1; g_16_1=g_uv_16_1; b_16_1=b_uv_16_1; \
	r_16_2=r_uv_16_2; g_16_2=g_uv_16_2; b_16_2=b_uv_16_2; \
	\
	y = LOAD_SI128((const __m128i*)(y_ptr2)); \
	y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128()); \
	y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128()); \
	\
	ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2) \
	\
	__m128i r_8_21 = _mm_packus_epi16(r_16_1, r_16_2); \
	__m128i g_8_21 = _mm_packus_epi16(g_16_1, g_16_2); \
	__m128i b_8_21 = _mm_packus_epi16(b_16_1, b_16_2); \
	\
	/* process last 16 pixels of first line */\
	u_16 = _mm_unpackhi_epi8(u, _mm_setzero_si128()); \
	v_16 = _mm_unpackhi_epi8(v, _mm_setzero_si128()); \
	u_16 = _mm_add_epi16(u_16, _mm_set1_epi16(-128)); \
	v_16 = _mm_add_epi16(v_16, _mm_set1_epi16(-128)); \
	\
	UV2RGB_16(u_16, v_16, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2) \
	r_uv_16_1=r_16_1; g_uv_16_1=g_16_1; b_uv_16_1=b_16_1; \
	r_uv_16_2=r_16_2; g_uv_16_2=g_16_2; b_uv_16_2=b_16_2; \
	\
	y = LOAD_SI128((const __m128i*)(y_ptr1+16)); \
	y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128()); \
	y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128()); \
	\
	ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2) \
	\
	__m128i r_8_12 = _mm_packus_epi16(r_16_1, r_16_2); \
	__m128i g_8_12 = _mm_packus_epi16(g_16_1, g_16_2); \
	__m128i b_8_12 = _mm_packus_epi16(b_16_1, b_16_2); \
	\
	/* process last 16 pixels of second line */\
	r_16_1=r_uv_16_1; g_16_1=g_uv_16_1; b_16_1=b_uv_16_1; \
	r_16_2=r_uv_16_2; g_16_2=g_uv_16_2; b_16_2=b_uv_16_2; \
	\
	y = LOAD_SI128((const __m128i*)(y_ptr2+16)); \
	y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128()); \
	y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128()); \
	\
	ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2) \
	\
	__m128i r_8_22 = _mm_packus_epi16(r_16_1, r_16_2); \
	__m128i g_8_22 = _mm_packus_epi16(g_16_1, g_16_2); \
	__m128i b_8_22 = _mm_packus_epi16(b_16_1, b_16_2); \
	\
	__m128i rgb_1, rgb_2, rgb_3, rgb_4, rgb_5, rgb_6; \
	\
	PACK_RGB24_32(r_8_11, r_8_12, g_8_11, g_8_12, b_8_11, b_8_12, rgb_1, rgb_2, rgb_3, rgb_4, rgb_5, rgb_6) \
	SAVE_SI128((__m128i*)(rgb_ptr1), rgb_1); \
	SAVE_SI128((__m128i*)(rgb_ptr1+16), rgb_2); \
	SAVE_SI128((__m128i*)(rgb_ptr1+32), rgb_3); \
	SAVE_SI128((__m128i*)(rgb_ptr1+48), rgb_4); \
	SAVE_SI128((__m128i*)(rgb_ptr1+64), rgb_5); \
	SAVE_SI128((__m128i*)(rgb_ptr1+80), rgb_6); \
	\
	PACK_RGB24_32(r_8_21, r_8_22, g_8_21, g_8_22, b_8_21, b_8_22, rgb_1, rgb_2, rgb_3, rgb_4, rgb_5, rgb_6) \
	SAVE_SI128((__m128i*)(rgb_ptr2), rgb_1); \
	SAVE_SI128((__m128i*)(rgb_ptr2+16), rgb_2); \
	SAVE_SI128((__m128i*)(rgb_ptr2+32), rgb_3); \
	SAVE_SI128((__m128i*)(rgb_ptr2+48), rgb_4); \
	SAVE_SI128((__m128i*)(rgb_ptr2+64), rgb_5); \
	SAVE_SI128((__m128i*)(rgb_ptr2+80), rgb_6); \


void yuv420_rgb24_sse(
	uint32_t width, uint32_t height, 
	const uint8_t *Y, const uint8_t *U, const uint8_t *V, uint32_t Y_stride, uint32_t UV_stride, 
	uint8_t *RGB, uint32_t RGB_stride, 
	YCbCrType yuv_type)
{
	#define LOAD_SI128 _mm_load_si128
	#define SAVE_SI128 _mm_stream_si128
	const YUV2RGBParam *const param = &(YUV2RGB[yuv_type]);
	
	uint32_t x, y;
	for(y=0; y<(height-1); y+=2)
	{
		const uint8_t *y_ptr1=Y+y*Y_stride,
			*y_ptr2=Y+(y+1)*Y_stride,
			*u_ptr=U+(y/2)*UV_stride,
			*v_ptr=V+(y/2)*UV_stride;
		
		uint8_t *rgb_ptr1=RGB+y*RGB_stride,
			*rgb_ptr2=RGB+(y+1)*RGB_stride;
		
		for(x=0; x<(width-31); x+=32)
		{
			YUV2RGB_32
			
			y_ptr1+=32;
			y_ptr2+=32;
			u_ptr+=16; 
			v_ptr+=16;
			rgb_ptr1+=96;
			rgb_ptr2+=96;
		}
	}
	#undef LOAD_SI128
	#undef SAVE_SI128
}

void yuv420_rgb24_sseu(
	uint32_t width, uint32_t height, 
	const uint8_t *Y, const uint8_t *U, const uint8_t *V, uint32_t Y_stride, uint32_t UV_stride, 
	uint8_t *RGB, uint32_t RGB_stride, 
	YCbCrType yuv_type)
{
	#define LOAD_SI128 _mm_loadu_si128
	#define SAVE_SI128 _mm_storeu_si128
	const YUV2RGBParam *const param = &(YUV2RGB[yuv_type]);
	
	uint32_t x, y;
	for(y=0; y<(height-1); y+=2)
	{
		const uint8_t *y_ptr1=Y+y*Y_stride,
			*y_ptr2=Y+(y+1)*Y_stride,
			*u_ptr=U+(y/2)*UV_stride,
			*v_ptr=V+(y/2)*UV_stride;
		
		uint8_t *rgb_ptr1=RGB+y*RGB_stride,
			*rgb_ptr2=RGB+(y+1)*RGB_stride;
		
		for(x=0; x<(width-31); x+=32)
		{
			YUV2RGB_32
			
			y_ptr1+=32;
			y_ptr2+=32;
			u_ptr+=16; 
			v_ptr+=16;
			rgb_ptr1+=96;
			rgb_ptr2+=96;
		}
	}
	#undef LOAD_SI128
	#undef SAVE_SI128
}
#endif //__SSE2__

