#ifndef SPECTRUMPAINTER_HPP
#define SPECTRUMPAINTER_HPP

#include <SDL_image.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

struct Settings
{
	Settings() {
		sampleRate = 44100;
		channels = 2;
		fftSize = 4096;
		windowInc = 200;
		tradeoff = 7;
		upperFreqLimit = 7000.0;
		ampScale = 1.0;
		labels = true;
		font = NULL;
		computeHelper();
	}

	void computeHelper()
	{
		freqResolution = float(sampleRate) / fftSize;
		timeResolution = float(windowInc) / sampleRate;
	}
	
	int sampleRate, channels;
	int fftSize, windowInc;
	float tradeoff;
	float upperFreqLimit;
	float timeResolution, freqResolution;
	float ampScale;
	bool labels;
	TTF_Font *font;
};


class SpectrumPainter
{
public:
	SpectrumPainter(SDL_Surface *imageSurface, const Settings &settings);
	void feedWithInput(const vector<float> &input);
	void reset();
	static SDL_Surface* audioToImage(const vector<Sint16> &audioData, const Settings &settings);
	void drawLabeling(SDL_Surface *surface);	
private:
	void frequencyAnalysis(const vector<float> &block, vector<float> &spectrum);
	void drawSpectrogram(const vector< vector<float> > &spectrums);
	void drawColumn(const vector<float> &spectrum, int xpos);
	float windowFunc(float x);
	float logarithmicScale(float y);

	void getColor(float x, float &r, float &g, float &b);
	Uint32 getColorSDL(SDL_PixelFormat *format, float x);
	void setPixel32(SDL_Surface *surface, int x, int y, Uint32 color);

	vector<float> block, window;
	vector< vector<float> > spectrums;
	int blockPosition, cursorPosition, samplesProcessed, scrolledTotal;

	Settings settings;
	SDL_Surface *imageSurface;
};


class Error
{
public:
	Error(const char *str)
	{
		message = str;
	}

	template<class T> static void raiseIfNull(const T val, const char *str)
	{
		if(!val) throw Error(str);
	}

	template<class T> static void raiseIfNotNull(const T val, const char *str)
	{
		if(val) throw Error(str);
	}

	inline const char* getMessage()
	{
		return message;
	}
private:
	const char *message;
};


template<class T>
std::string toString(T object)
{
	std::string r;
	std::stringstream s;
	s << object;
	s >> r;
	return r;	
}


#endif
