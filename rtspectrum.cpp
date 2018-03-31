#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <SDL_image.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <SDL_audio.h>
#include <sndfile.h>
#include "common.hpp"


using namespace std;

void rdft(int n, int isgn, float *a);

template<class T>
std::string toString(T object)
{
	std::string r;
	std::stringstream s;
	s << object;
	s >> r;
	return r;	
}

class Error
{
public:
	template<class T> static void raiseIfNull(const T val, const char *str)
	{
		if(!val) throw str;
	}

	template<class T> static void raiseIfNotNull(const T val, const char *str)
	{
		if(val) throw str;
	}
};


SDL_Renderer *renderer;
SDL_Window *sdlWindow;
SDL_Surface *screenSurface, *imageSurface;
SDL_AudioSpec want, have;
SDL_AudioDeviceID audioDevice;
TTF_Font *font;
bool quit = false;
bool recording = false;

const int sampleRate = 44100;
const int channels = 2;

typedef Sint16 SampleFormat;
vector<SampleFormat> audioData;

int processed = 0;

void audioCallback(void *userdata, Uint8 *data, int length)
{
	if(!recording) return;
	int numSamples = length / sizeof(SampleFormat);
	SampleFormat *samples = reinterpret_cast<SampleFormat*>(data);
	for(int i = 0; i < numSamples; ++i)
		audioData.push_back(samples[i]);
}


const char *title = "Spektrum";

int fftSize = 4096, windowInc = 200, tradeoff = 3.0;
vector<float> block, window, fftdata, spectrum;
vector< vector<float> > spectrumList;

int screenWidth = 1200;
int screenHeight = 650;
int cursorPosition = 0;

float freqResolution, timeResolution;

void frequencyAnalysis(const vector<float> &block, vector<float> &spectrum)
{
	for(int i = 0; i < fftSize; ++i)
		fftdata[i] = block[i] * window[i];
	rdft(fftSize, 1, &fftdata[0]);
	for(int i = 0; i < fftSize / 2; ++i)
		spectrum[i] = hypot(fftdata[i * 2], fftdata[i * 2 + 1]) * 2.0 / fftSize;
}

float windowFunc(float x)
{
	float u = 0.5 / tradeoff;
	if(x >= 0.5 - u && x <= 0.5 + u)
		return sinf((x - (0.5 - u)) * tradeoff * M_PI) * sqrt(tradeoff);
	else
		return 0;
}


void initializeSpectrum()
{
	freqResolution = float(sampleRate) / fftSize;
	timeResolution = float(windowInc) / sampleRate;
	
	block.resize(fftSize);
	spectrum.resize(fftSize / 2);
	window.resize(fftSize);
	fftdata.resize(fftSize);
	for(long i = 0; i < fftSize; ++i)
		window[i] = windowFunc(float(i) / fftSize);
}


void processIncomingData()
{
	while(audioData.size() >= processed + windowInc * channels)
	{
		for(long i = 0; i < fftSize - windowInc; ++i)
			block[i] = block[i + windowInc];

		SDL_LockAudioDevice(audioDevice);
		for(int i = 0; i < windowInc; ++i)
		{
			float monoSample = 0.0;
			for(int j = 0; j < channels; ++j)
				monoSample += audioData[processed + i * channels + j];
			block[i + fftSize - windowInc] = monoSample / (32768.0f * channels);
		}
		processed += windowInc * channels;
		SDL_UnlockAudioDevice(audioDevice);

		frequencyAnalysis(block, spectrum);
		spectrumList.push_back(spectrum);
	}
}



void initializeSDL()
{
	// Initialize SDL
	int result;
	result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	Error::raiseIfNotNull(result, "SDL_Init failed");

	// initalize Video
	sdlWindow = SDL_CreateWindow("Spectrum", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screenWidth, screenHeight, SDL_WINDOW_SHOWN);
	Error::raiseIfNull(sdlWindow, "SDL_CreateWindow failed");
	screenSurface = SDL_GetWindowSurface(sdlWindow);
	imageSurface = SDL_CreateRGBSurface(0, screenWidth, screenHeight, 24, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);	
	Error::raiseIfNull(imageSurface, "SDL_CreateRGBSurface failed");

	// Initialize Image
	result = IMG_Init(IMG_INIT_PNG);
	Error::raiseIfNull(result & IMG_INIT_PNG, "IMG_Init failed");

	// Initialize Fonts
	result = TTF_Init();
	Error::raiseIfNotNull(result, "TTF_Init failed");
    font = TTF_OpenFont("OpenSans-Regular.ttf", 16);
	Error::raiseIfNull(font, "TTF_OpenFont failed");

	// Initialize Audio
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = sampleRate;
	want.format = AUDIO_S16SYS;
	want.channels = channels;
	want.samples = 1024;
	want.callback = audioCallback;
	
	audioDevice = SDL_OpenAudioDevice(NULL, 1, &want, &have, 0);
	Error::raiseIfNull(audioDevice, "SDL_AudioDevice failed");
	SDL_PauseAudioDevice(audioDevice, 0);
}

void finalizeSDL()
{
	TTF_CloseFont(font);
	TTF_Quit();
	
	SDL_FreeSurface(imageSurface);
    SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();
}

void drawLabels()
{
	string text = string("Last ") + toString(timeResolution * screenWidth) + " sec, ";
	text += string("0 - ") + toString(freqResolution * screenHeight) + " Hz";
	text += " Keys: Space = record, s = Save, r = Reset";
	SDL_Color textColor = { 255, 255, 255, 255 };
	SDL_Surface* textSurface = TTF_RenderText_Blended(font, text.c_str(), textColor);
	
	Error::raiseIfNull(textSurface, "TTF_RenderText_Solid failed");
	SDL_Rect dstrect;
	dstrect.x = 0;
	dstrect.y = 0;			
	SDL_BlitSurface(textSurface, NULL, screenSurface, &dstrect);
	SDL_FreeSurface(textSurface);
}




void drawSpectrum()
{	
	SDL_Rect srcrect, dstrect;

	int move = spectrumList.size();
	if(move >= imageSurface->w) move = imageSurface->w;
	
	if(cursorPosition > imageSurface->w - move)
	{
		srcrect.x = move;
		srcrect.y = 0;
		srcrect.w = imageSurface->w;
		srcrect.h = imageSurface->h;
		dstrect.x = 0;
		dstrect.y = 0;			
		SDL_BlitSurface(imageSurface, &srcrect, imageSurface, &dstrect);
		cursorPosition -= move;
	}

	SDL_LockSurface(imageSurface);
	Uint8 *pixels = reinterpret_cast<Uint8*>(imageSurface->pixels);

	for(int i = 0; i < move; ++i)
		for(int y = 0; y < imageSurface->h; ++y) {
			if(y >= spectrum.size()) break;
			float a = logarithmicScale(spectrumList[i][y] * sqrt(y));
			setPixel32(imageSurface, cursorPosition + i, imageSurface->h - y - 1,
				getColorSDL(imageSurface->format, a));
		}
	cursorPosition += move; 

	SDL_UnlockSurface(imageSurface);
	spectrumList.clear();

	SDL_BlitSurface(imageSurface, NULL, screenSurface, NULL);
}

string timeString(time_t t)
{
	tm *now = localtime(&t);
	stringstream s;
	string output;
	s << now->tm_year + 1900;
	s << setfill('0') << setw(2) << now->tm_mon + 1;
	s << setfill('0') << setw(2) << now->tm_mday;
	s << setfill('0') << setw(2) << now->tm_hour;
	s << setfill('0') << setw(2) << now->tm_min;
	s << setfill('0') << setw(2) << now->tm_sec;
	s >> output;
	return output;
}

void saveAudioRecording(const string &filename)
{
	cout << "Saving audio recording: " << filename << endl;
	
	SF_INFO sf_info;
	sf_info.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
	sf_info.samplerate = sampleRate;
	sf_info.channels = channels;
	SNDFILE *sf = sf_open(filename.c_str(), SFM_WRITE, &sf_info);
	Error::raiseIfNull(sf, "sf_open failed");

	for(int i = 0; i < audioData.size(); ++i)
		sf_write_short(sf, &audioData[i], 1);
	sf_close(sf);
}

void saveSpectrumImage(const string &filename)
{
	cout << "Saving spectrum image: " << filename << endl;
	
	vector<float> block(fftSize, 0.0);
	vector<float> spectrum(fftSize / 2.0, 0.0);

	int imageXpos = 0;
	int blockPosition = 0;
	int numFrames = audioData.size() / channels;

	const int upperFreqLimit = 7000.0;
	int imageWidth = (numFrames - fftSize) / windowInc + 1;
	int imageHeight = int(upperFreqLimit / freqResolution) + 1;
	if(imageHeight > fftSize / 2) imageHeight = fftSize / 2;
	if(imageWidth <= 0) imageWidth = 1;

	SDL_Surface *image = SDL_CreateRGBSurface(0, imageWidth, imageHeight, 24, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);
	Error::raiseIfNull(image, "SDL_CreateRGBSurface failed");
	SDL_LockSurface(image);
	
	for(int position = 0; position < numFrames; ++position)
	{
		if(position % sampleRate == 0) {
			cout << position / sampleRate << " "; cout.flush();}

		float monoSample = 0.0;
		for(int channel = 0; channel < channels; ++channel)
			monoSample += audioData[position * channels + channel];
		monoSample /= 32768.0f * channels;
		
		if(blockPosition == fftSize)
		{
			frequencyAnalysis(block, spectrum);

			for(int y = 0; y < imageHeight; ++y)
			{
				float a = logarithmicScale(spectrum[y] * sqrt(y));
				setPixel32(image, imageXpos, imageHeight - y - 1,
					getColorSDL(image->format, a));
			}
			imageXpos += 1;
			
			for(int i = 0; i < fftSize - windowInc; ++i) 
				block[i] = block[i + windowInc];
			blockPosition -= windowInc;
		}
	
		block[blockPosition] = monoSample;
		++blockPosition;
	}
	cout << endl;

	SDL_UnlockSurface(image);
	
	IMG_SavePNG(image, filename.c_str());
	SDL_FreeSurface(image);
}

void clearRecording()
{
	SDL_LockAudioDevice(audioDevice);
	processed = 0;
	audioData.clear();
	SDL_UnlockAudioDevice(audioDevice);
	
	cursorPosition = 0;
	block.assign(fftSize, 0);
	SDL_FillRect(imageSurface, NULL, SDL_MapRGB(imageSurface->format, 0, 0, 0));
}


void onKeyDown(const SDL_Event &event)
{
	switch(event.key.keysym.sym) {
		case SDLK_ESCAPE:
			quit = true;
			break;
		case SDLK_SPACE:
			recording = true;
			break;
		case SDLK_r:
			clearRecording();
			break;
		case SDLK_s:
			string timeStr = timeString(time(0));
			saveAudioRecording(string("recording-") + timeStr + ".ogg");
			saveSpectrumImage(string("recording-") + timeStr + ".png");
			break;
	}
}

void onKeyUp(const SDL_Event &event)
{
	switch(event.key.keysym.sym) {
		case SDLK_SPACE:
			recording = false;
			break;
	}
}


int main(int argc, char **argv)
{
	try {
	initializeSDL();
	initializeSpectrum();

	SDL_Event event;

	while(!quit)
	{
		int startTime = SDL_GetTicks();
	
		while(SDL_PollEvent(&event) != 0)
		{
			if(event.type == SDL_QUIT)
				quit = true;
			if(event.type == SDL_KEYDOWN && !event.key.repeat)
				onKeyDown(event);
			if(event.type == SDL_KEYUP)
				onKeyUp(event);
		}

		processIncomingData();
		drawSpectrum();
		drawLabels();

		const float deltaTime = 0.04; 
		int endTime = SDL_GetTicks();
		if(endTime - startTime < deltaTime * 1000)
			SDL_Delay(deltaTime * 1000 - (endTime - startTime));
		SDL_UpdateWindowSurface(sdlWindow);
	}
	}
	catch(const char *e) {
		cout << "Fehler: " << e << endl;
	}
	finalizeSDL();
}
