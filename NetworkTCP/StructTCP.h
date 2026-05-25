#pragma once
#include "zlib/zlib.h"

struct Zip {
	z_stream* y;
	z_stream* j;

	Zip() {
		y = new z_stream();
		deflateInit(y, Z_DEFAULT_COMPRESSION);
		j = new z_stream();
		inflateInit(j);
	}

	~Zip() {
		delete y;
		delete j;
	}
};

struct DataHeader
{
	unsigned int Key;
	unsigned int Size;
};