#ifndef COMMON_HPP
#define COMMON_HPP

#include <SDL.h>
#include <SDL_surface.h>

void getColor(float x, float &r, float &g, float &b);
Uint32 getColorSDL(SDL_PixelFormat *format, float x);
void setPixel32(SDL_Surface *surface, int x, int y, Uint32 color);
float logarithmicScale(float y);

#endif
