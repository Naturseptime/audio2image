#include "spectrumpainter.hpp"
#include <sndfile.h>

using namespace std;

void showHelp(const Settings &settings)
{
	printf("Syntax: audio2image inputfile outputfile [fftsize] [windowinc] [tradeoff] [upperfreq] [labels]\n");
	printf("\tfftsize    = FFT window size (default %d)\n", settings.fftSize);
	printf("\twindowinc = FFT window movement (default %d)\n", settings.windowInc);
	printf("\ttradeoff  = frequency/time-resolution-tradeoff (default %f)\n", settings.tradeoff);
	printf("\t\t(1 for high resolution in frequency domain)\n");
	printf("\t\t(10 for high resolution in time domain)\n");
	printf("\tupperfreq = maximal frequency in image (default %f)\n", settings.upperFreqLimit);
	printf("\tlabels = 1 with labels, 0 without labels (default 1)\n", settings.labels);
}

int main(int argc, char **argv)
{
	Settings settings;
	
	char *inputfile, *outputfile;
	if(argc < 3) {
		showHelp(settings);
		return 1;
	}
	
	inputfile = argv[1];
	outputfile = argv[2];
	
	if(argc >= 4) settings.fftSize = atoi(argv[3]);
	if(argc >= 5) settings.windowInc = atoi(argv[4]);
	if(argc >= 6) settings.tradeoff = atof(argv[5]);
	if(argc >= 7) settings.upperFreqLimit = atoi(argv[6]);
	if(argc >= 8) settings.labels = atoi(argv[7]);

	if(settings.fftSize <= 1 || settings.windowInc <= 0 ||
		settings.upperFreqLimit <= 0 || settings.tradeoff < 1) {
		printf("Error: parameters are invalid!\n"); return 1;}
	
	if(settings.fftSize & (settings.fftSize - 1) != 0) {
		printf("Error: windowsize must be power of 2!\n"); return 1;}

	SF_INFO sfinfo;
	SNDFILE *sf = sf_open(inputfile, SFM_READ, &sfinfo);
	if(sf == NULL) {printf("Error: Could not read file %s.\n", argv[1]); return 1;}

	settings.sampleRate = sfinfo.samplerate;
	settings.channels = sfinfo.channels;
	settings.computeHelper();

	vector<Sint16> audioData(sfinfo.frames * sfinfo.channels, 0.0);
	for(int i = 0; i < sfinfo.frames; ++i)
		sf_readf_short(sf, &audioData[i * sfinfo.channels], 1);

	// Initialize SDL
	int result;
	result = SDL_Init(SDL_INIT_VIDEO);
	Error::raiseIfNotNull(result, "SDL_Init failed");

	// Initialize Image
	result = IMG_Init(IMG_INIT_PNG);
	Error::raiseIfNull(result & IMG_INIT_PNG, "IMG_Init failed");
	
	// Initialize Fonts
	result = TTF_Init();
	Error::raiseIfNotNull(result, "TTF_Init failed");
    settings.font = TTF_OpenFont("OpenSans-Regular.ttf", 16);
	Error::raiseIfNull(settings.font, "TTF_OpenFont failed");

	SDL_Surface *image = SpectrumPainter::audioToImage(audioData, settings);
	IMG_SavePNG(image, outputfile);
	SDL_FreeSurface(image);

	return 0;
}
