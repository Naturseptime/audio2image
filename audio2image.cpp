#include <stdio.h>
#include <sndfile.h>
#include <vector>
#include <SDL_image.h>
#include <SDL_surface.h>
#include "common.hpp"

using namespace std;


void rdft(int n, int isgn, float *a);

int windowSize = 4096, windowInc = 100, upperFreqLimit = 7000.0, tradeoff = 3.0;

vector<float> window, fftdata;

void frequencyAnalysis(const vector<float> &block, vector<float> &spectrum)
{
	for(int i = 0; i < windowSize; ++i)
		fftdata[i] = block[i] * window[i];
	rdft(windowSize, 1, &fftdata[0]);
	for(int i = 0; i < windowSize / 2; ++i)
		spectrum[i] = hypot(fftdata[i * 2], fftdata[i * 2 + 1]) * 2.0 / windowSize;
}

float windowFunc(float x)
{
	float u = 0.5 / tradeoff;
	if(x >= 0.5 - u && x <= 0.5 + u)
		return sinf((x - (0.5 - u)) * tradeoff * M_PI) * sqrt(tradeoff);
	else
		return 0;
}

//float windowFunc(float x)
//{
	//return sinf(x * M_PI) * 2;
//}

void initialize()
{
	window.resize(windowSize);
	fftdata.resize(windowSize);
	for(long i = 0; i < windowSize; ++i)
		window[i] = windowFunc(float(i) / windowSize);
}

int main(int argc, char **argv)
{
	char *inputfile, *outputfile;
	if(argc < 3) {
		printf("Syntax: audio2image inputfile outputfile [windowsize] [windowinc] [tradeoff] [upperfreq]\n");
		printf("\twindowsize = FFT window size (default %d)\n", windowSize);
		printf("\twindowinc  = FFT window movement (default %d)\n", windowInc);
		printf("\ttradeoff   = frequency/time-resolution-tradeoff (default %d)\n", tradeoff);
		printf("\t\t(1 for high resolution in frequency domain)\n");
		printf("\t\t(10 for high resolution in time domain)\n");
		printf("\tupperfreq  = maximal frequency in image (default %d)\n", upperFreqLimit);
		return 1;
	}
	
	inputfile = argv[1];
	outputfile = argv[2];
	if(argc >= 4) windowSize = atoi(argv[3]);
	if(argc >= 5) windowInc = atoi(argv[4]);
	if(argc >= 6) tradeoff = atof(argv[5]);
	if(argc >= 7) upperFreqLimit = atoi(argv[6]);

	
	SF_INFO sfinfo;
	SNDFILE *sf = sf_open(inputfile, SFM_READ, &sfinfo);
	if(sf == NULL) {printf("Error: Could not read file %s.\n", argv[1]); return 1;}

	if(windowSize <= 1 || windowInc <= 0 || upperFreqLimit <= 0 || tradeoff < 1) {
		printf("Error: parameters are invalid!\n"); return 1;}
	
	if(windowSize & (windowSize - 1) != 0) {
		printf("Error: windowsize must be power of 2!\n"); return 1;}

	initialize();

	float freqResolution = float(sfinfo.samplerate) / windowSize;
	double timeResolution = float(windowInc) / sfinfo.samplerate;
	
	printf("Frames: %d\n", sfinfo.frames);
	printf("Length: %f sec\n", float(sfinfo.frames) / sfinfo.samplerate);
	printf("Frequency resolution: %f Hz\n", freqResolution);
	printf("Time resolution: %f sec\n", timeResolution);
	int imageWidth = (sfinfo.frames - windowSize) / windowInc + 2;
	int imageHeight = int(upperFreqLimit / freqResolution) + 1;
	if(imageHeight > windowSize / 2) imageHeight = windowSize / 2;

	if(imageWidth > 200000 || imageHeight > 5000 || imageWidth * imageHeight > 1000000000) {
		printf("Error: image would be too large!\n"); return 1;}
	

	SDL_Init(SDL_INIT_VIDEO);
	IMG_Init(IMG_INIT_PNG);
	
	SDL_Surface *image = SDL_CreateRGBSurface(0, imageWidth, imageHeight, 24, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);
	SDL_LockSurface(image);
	
	vector<float> frame(sfinfo.channels, 0);
	vector<float> block(windowSize);
	vector<float> spectrum(windowSize / 2);

	printf("Calculate image of size %dx%d:\n", imageWidth, imageHeight);
	int position = 0, blockPos = 0, imageXpos = 0;
	while(position < sfinfo.frames) {
		if(position % sfinfo.samplerate == 0) {
			printf("%d ", position / sfinfo.samplerate); fflush(stdout);}
		int framesRead = sf_readf_float(sf, &frame[0], 1);
		if(framesRead == 0) break; // OGG Bug Workaround
		++position;
		float monoSample = 0.0;
		for(int channel = 0; channel < sfinfo.channels; ++channel)
			monoSample += frame[channel];
		monoSample /= sfinfo.channels;

		if(blockPos == windowSize)
		{
			frequencyAnalysis(block, spectrum);

			for(int y = 0; y < imageHeight; ++y)
			{
				float a = logarithmicScale(spectrum[y] * sqrt(y));
				setPixel32(image, imageXpos, imageHeight - y - 1,
					getColorSDL(image->format, a));
			}
			imageXpos += 1;

			
			for(int i = 0; i < windowSize - windowInc; ++i) 
				block[i] = block[i + windowInc];
			blockPos -= windowInc;
		}
		
		//printf("%d\n", imageXpos);
		block[blockPos] = monoSample;
		++blockPos;
	}
	printf("\nComplete!\n");

	//double hpoints = 1, vpoints = 500;
	//for(int i = 0; i < imageWidth * timeResolution / hpoints; ++i)
		//for(int j = 0; j < imageHeight * freqResolution / vpoints; ++j)
		//{
			//int x = i / timeResolution * hpoints;
			//int y = j / freqResolution * vpoints;
			//setPixel32(image, x, imageHeight - y - 1, 0x888888);
		//}
				
	
	SDL_UnlockSurface(image);
	
	IMG_SavePNG(image, outputfile);
	SDL_FreeSurface(image);

	return 0;
}
