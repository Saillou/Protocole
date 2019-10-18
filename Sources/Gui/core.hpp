#pragma once

#include <vector>

struct Point {
	Point(int x_ = 0, int y_ = 0) : 
		x(x_), 
		y(y_)
	{	};
	
	int x;
	int y;
};
struct Size {
	Size(int width_ = 0, int height_ = 0) : 
		width(width_), 
		height(height_)
	{	};
	
	int width;
	int height;
};
struct Color {
	Color(int r_ = 0, int g_ = 0, int b_ = 0) : 
		r(r_), 
		g(g_),
		b(b_)
	{	};
	
	int r;
	int g;
	int b;
};
struct Frame {
	Frame(int width_ = 0, int height_ = 0, int channels_ = 0) : 
		width(width_), 
		height(height_),
		channels(channels_)
	{	
		image.resize(width_*height_*channels_);
	};
	
	int width;
	int height;
	int channels;
	std::vector<unsigned char> image;
	
	const unsigned char* data() const {
		return image.data();
	}
	const int bitsPerSample() const {
		return 8;
	}
	const int bytesPerRow() const {
		return width * channels;
	}
	const int area() const {
		return width * height;
	}
	const int total() const {
		return width * height * channels;
	}
};
