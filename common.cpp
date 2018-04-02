#include "common.hpp"

void getColor(float x, float &r, float &g, float &b)
{
	const int csamples = 6;
	float colors[csamples][3] = {
		{0.0, 0.0, 0.0},
		{0.0, 0.0, 0.75},
		{0.0, 0.75, 0.0},
		{0.8, 0.8, 0.0},
		{0.9, 0.2, 0.2},
		{1.0, 1.0, 1.0}};

	x *= csamples - 1;
	int xi = int(x);
	float xf = x - xi;
	if(xi < 0) {xi = 0; xf = 0;}
	if(xi >= csamples - 1) {xi = csamples - 2; xf = 1.0;}
	
	r = colors[xi][0] * (1.0 - xf) + colors[xi + 1][0] * xf;
	g = colors[xi][1] * (1.0 - xf) + colors[xi + 1][1] * xf;
	b = colors[xi][2] * (1.0 - xf) + colors[xi + 1][2] * xf;
}

Uint32 getColorSDL(SDL_PixelFormat *format, float x)
{
	float r, g, b;
	getColor(x, r, g, b);
	if(r < 0.0f) r = 0.0f;
	if(r > 1.0f) r = 1.0f;
	if(g < 0.0f) g = 0.0f;
	if(g > 1.0f) g = 1.0f;
	if(b < 0.0f) b = 0.0f;
	if(b > 1.0f) b = 1.0f;
	return SDL_MapRGB(format, int(r * 255), int(g * 255), int(b * 255));
}

float logarithmicScale(float y)
{
	const float min = 1e-2;
	const float max = 1e-0f;

	y += min;
	return (logf(y) - logf(min)) / (logf(max) - logf(min));
}


void setPixel32(SDL_Surface *surface, int x, int y, Uint32 color)
{
	if(x < 0 || x >= surface->w || y < 0 || y >= surface->h) {printf("Overflow: %d %d\n", x, y); exit(1);}
	Uint8* p = reinterpret_cast<Uint8*>(surface->pixels) + y * surface->pitch + x * surface->format->BytesPerPixel;
	p[0] = color & 0xff;
	p[1] = (color >> 8) & 0xff;
	p[2] = (color >> 16) & 0xff;
}

