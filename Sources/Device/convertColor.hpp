#pragma once

#include <cstring> // memcpy

namespace Convert {
/* Example image:
		P00 P01 P02 P03 ..
		P10 P11 P12 P13 ..
		P20 P21 P22 P23 ..
		P30 P31 P32 P33 ..
		
	-> BGR:
		B00 G00 R00		B01 G01 G01		B02 G02 R02		B03 G03 G03
		B10 G10 R10		...
		B20 G20 R20		...
		B30 G30 R30		...
		....

	-> YUV(420):
		Y00 Y01 Y02 Y03 ..	|	U00 	.	U02	.	|	V00 	.	V02	.
		Y10 Y11 Y12 Y13 ..	|	 .		.	 .		.	|	 .		.	 .		.
		Y20 Y21 Y22 Y23 ..	|	U20	.	U22	.	|	V20	.	V22	.
		Y30 Y31 Y32 Y33 ..	|	.		.	 .		.	|	 .		.	 .		.
		...
		
	=> With, Y', R',B',G' in [0, 1] :
		Y' = 0,299*R' 	+ 0,587*G' 		+ 0,114*B' 
		U  = −0,14713*R' − 0,28886*G' 	+ 0,436*B' 
		V  = 0,615*R' 	− 0,51498*G' 	– 0,10001*B' 
		
	=> 
		B' = Y' + 2,03211*U 
		G' = Y' − 0,39465*U − 0,58060*V
		R' = Y' + 1,13983*V
*/

static void yuv422ToYuv420(unsigned char *yuv422, unsigned char *yuv420, size_t width, size_t height) {		
	// Y don't change.												size422 = size420 = area.
	// U and V have width, but height is divided by 2. size422 = area /2 -> size420 = area /4
	
	size_t area = width * height;
	
	// Copy Y
	memcpy(yuv420, yuv422, area);
	
	// Copy half U, V
	for(size_t ih = 0; ih < height/2; ih++) 
		memcpy(yuv420 + area + ih*width/2, yuv422 + area + ih*width, width/2);
}

static void bgr24ToYuv420(unsigned char *bgr, unsigned char **yuv, size_t width, size_t height) {		
	// Param
	const size_t INC_BGR = 3;		 					// Space between 2 BGR pixels
	const size_t INC_LINE_BGR = width*INC_BGR; 	// Stride
	const size_t OFF_B = 0; 							// Offsets
	const size_t OFF_G = 1;
	const size_t OFF_R = 2;
	
	// Output
	unsigned char* py = yuv[0];
	unsigned char* pu = yuv[1];
	unsigned char* pv = yuv[2];
	
	// Translation
	for(size_t iH = 0; iH < height-1; iH += 2) {
		for(size_t iW = 0; iW < width; iW += 2) {
			// Fetch values
			size_t p = (iW + iH*width);
			size_t q = (iW/2 + iH*width/4);
			size_t i = p*INC_BGR;
			
			// Input
			const unsigned char b[4] = {	bgr[i+OFF_B], 						bgr[i+INC_BGR+OFF_B], 
													bgr[i+INC_LINE_BGR+OFF_B], 	bgr[i+INC_LINE_BGR+INC_BGR+OFF_B]};

			const unsigned char g[4] = {	bgr[i+OFF_G],						bgr[i+INC_BGR+OFF_G], 
													bgr[i+INC_LINE_BGR+OFF_G], 	bgr[i+INC_LINE_BGR+INC_BGR+OFF_G]};
													
			const unsigned char r[4] = {	bgr[i+OFF_R], 						bgr[i+INC_BGR+OFF_R], 
													bgr[i+INC_LINE_BGR+OFF_R], 	bgr[i+INC_LINE_BGR+INC_BGR+OFF_R]};
			
			// Output
			unsigned char* pY[4] = {&py[p], 			&py[p+1],
											&py[p+width],	&py[p+width+1]};
											
			unsigned char* pU 	= &pu[q];
			unsigned char* pV 	= &pv[q];
			int u = 0;
			int v = 0;
			
			// Compute
			for(int j = 0; j < 4; j++) {
				*pY[j] 	= 	((66*r[j] + 129*g[j] + 25*b[j]) >> 8) + 16;
				u 			+= ((-38*r[j] + -74*g[j] + 112*b[j]) >> 8) + 128;
				v			+= ((112*r[j] + -94*g[j] + -18*b[j]) >> 8) + 128;
			}
			*pU = (unsigned char)(u >> 2);
			*pV = (unsigned char)(v >> 2);
			// ....
			
		}
	}	
	// ...
}
static void yuv420ToBgr24(unsigned char **yuv, unsigned char *bgr, size_t stride, size_t width, size_t height) {
	// Param
	const size_t INC_BGR = 3;		 					// Space between 2 BGR pixels
	const size_t INC_LINE_BGR = width*INC_BGR; 	// Stride
	const size_t OFF_B = 0; 							// Offsets
	const size_t OFF_G = 1;
	const size_t OFF_R = 2;
	
	// Output
	unsigned char* py = yuv[0];
	unsigned char* pu = yuv[1];
	unsigned char* pv = yuv[2];
	
	// Translation
	for(size_t iH = 0; iH < height-1; iH += 2) {
		for(size_t iW = 0; iW < width; iW += 2) {
			// Fetch values
			size_t p = (iW + iH*stride);
			size_t q = (iW/2 + iH*stride/4);
			size_t i = (iW + iH*width)*INC_BGR;
			
			// Input
			const unsigned char y[4] = {	py[p], 			py[p+1],
													py[p+stride],	py[p+stride+1]};
													
			
			const unsigned char u = pu[q];
			const unsigned char v = pv[q];
			
			// Output
			unsigned char* pB[4] = {	&bgr[i+OFF_B], 			 		&bgr[i+INC_BGR+OFF_B], 
												&bgr[i+INC_LINE_BGR+OFF_B], 	&bgr[i+INC_LINE_BGR+INC_BGR+OFF_B]};

			unsigned char* pG[4] = {	&bgr[i+OFF_G],			 		&bgr[i+INC_BGR+OFF_G], 
												&bgr[i+INC_LINE_BGR+OFF_G], 	&bgr[i+INC_LINE_BGR+INC_BGR+OFF_G]};
													
			unsigned char* pR[4] = {	&bgr[i+OFF_R], 			 		&bgr[i+INC_BGR+OFF_R], 
												&bgr[i+INC_LINE_BGR+OFF_R], 	&bgr[i+INC_LINE_BGR+INC_BGR+OFF_R]};
			
			// Compute
			int D = u - 128;
			int E = v - 128;
			for(int j = 0; j < 4; j++) {
				int C = y[j] - 16;
				
				int r = (298 * C + 409 * E + 128) >> 8;
				int g = (298 * C - 100 * D - 208 * E + 128) >> 8;
				int b = (298 * C + 516 * D + 128) >> 8;
				
				*pR[j] = (unsigned char)(r < 255 ? (r > 0 ? r : 0) : 255);
				*pG[j] = (unsigned char)(g < 255 ? (g > 0 ? g : 0) : 255);
				*pB[j] = (unsigned char)(b < 255 ? (b > 0 ? b : 0) : 255);
			}
			// ....
			
		}
	}	
	// ...
}

} // namespace Convert
