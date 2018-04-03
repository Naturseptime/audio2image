#include <SDL_image.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <SDL_audio.h>
#include <sndfile.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>

#include "spectrumpainter.hpp"

using namespace std;

template<class T>
std::string toString(T object)
{
	std::string r;
	std::stringstream s;
	s << object;
	s >> r;
	return r;	
}


class RTSpectrumApp
{
public:
	RTSpectrumApp();
	~RTSpectrumApp();	
	void run();
private:
	void initializeSDL();
	void finalizeSDL();
	
	void drawSpectrum();
	void drawLabels();

	void onKeyDown(const SDL_Event &event);
	void onKeyUp(const SDL_Event &event);

	void audioCallback(Uint8 *data, int length);
	friend void globalAudioCallback(void *userdata, Uint8 *data, int length);
	void processAudio();

	void clearRecording();
	void saveAudioRecording(const string &filename);
	void saveSpectrumImage(const string &filename);
	string timeString(time_t t);
	
	SDL_Renderer *renderer;
	SDL_Window *sdlWindow;
	SDL_Surface *screenSurface, *imageSurface;
	SDL_AudioSpec want, have;
	SDL_AudioDeviceID audioDevice;
	TTF_Font *font;
	bool quit, recording;

	int screenWidth, screenHeight;

	vector<Sint16> audioData;
	int audioProcessed;

	Settings settings;
	SpectrumPainter *spectrumPainter;
};


void globalAudioCallback(void *userdata, Uint8 *data, int length)
{
	reinterpret_cast<RTSpectrumApp*>(userdata)->audioCallback(data, length);
}

RTSpectrumApp::RTSpectrumApp()
{
	quit = recording = false;

	screenWidth = 1200;
	screenHeight = int(settings.upperFreqLimit / settings.freqResolution) + 1;

	audioProcessed = 0;
	
	initializeSDL();
	spectrumPainter = new SpectrumPainter(imageSurface, settings);
}


RTSpectrumApp::~RTSpectrumApp()
{
	delete spectrumPainter;
	finalizeSDL();
}

void RTSpectrumApp::initializeSDL()
{
	// Initialize SDL
	int result;
	result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	Error::raiseIfNotNull(result, "SDL_Init failed");

	// Initalize Video
	sdlWindow = SDL_CreateWindow("Spectrum", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screenWidth, screenHeight, SDL_WINDOW_SHOWN);
	Error::raiseIfNull(sdlWindow, "SDL_CreateWindow failed");
	screenSurface = SDL_GetWindowSurface(sdlWindow);
	Error::raiseIfNull(screenSurface, "SDL_GetWindowSurface failed");
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
	want.freq = settings.sampleRate;
	want.format = AUDIO_S16SYS;
	want.channels = settings.channels;
	want.samples = 1024;
	want.userdata = this;
	want.callback = globalAudioCallback;
	
	audioDevice = SDL_OpenAudioDevice(NULL, 1, &want, &have, 0);
	Error::raiseIfNull(audioDevice, "SDL_AudioDevice failed");
	SDL_PauseAudioDevice(audioDevice, 0);
}

void RTSpectrumApp::finalizeSDL()
{
	TTF_CloseFont(font);
	TTF_Quit();
	
	SDL_FreeSurface(imageSurface);
    SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();
}

void RTSpectrumApp::run()
{
	SDL_Event event;

	while(!quit)
	{
		int startTime = SDL_GetTicks();
	
		while(SDL_PollEvent(&event) != 0) {
			if(event.type == SDL_QUIT)
				quit = true;
			if(event.type == SDL_KEYDOWN && !event.key.repeat)
				onKeyDown(event);
			if(event.type == SDL_KEYUP)
				onKeyUp(event);
		}

		processAudio();
		drawLabels();

		const float deltaTime = 0.04; 
		int endTime = SDL_GetTicks();
		if(endTime - startTime < deltaTime * 1000)
			SDL_Delay(deltaTime * 1000 - (endTime - startTime));
		SDL_UpdateWindowSurface(sdlWindow);
	}
}

void RTSpectrumApp::onKeyDown(const SDL_Event &event)
{
	switch(event.key.keysym.sym) {
		case SDLK_ESCAPE:
			quit = true;
			break;
		case SDLK_SPACE:
			recording = !recording;
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

void RTSpectrumApp::onKeyUp(const SDL_Event &event)
{
	switch(event.key.keysym.sym) {
		case SDLK_SPACE:
			//recording = false;
			break;
	}
}

void RTSpectrumApp::audioCallback(Uint8 *data, int bytes)
{
	if(recording) {
		int samples = bytes / sizeof(Sint16);
		for(int i = 0; i < samples; ++i)
			audioData.push_back(reinterpret_cast<Sint16*>(data)[i]);
	}
}

void RTSpectrumApp::processAudio()
{
	vector<float> input;
	SDL_LockAudioDevice(audioDevice);
	while(audioProcessed < audioData.size())
	{
		float monoSample = 0.0;
		for(int j = 0; j < settings.channels; ++j)
			monoSample += audioData[audioProcessed + j];
		monoSample /= 32768.0f * settings.channels;
		input.push_back(monoSample);
		audioProcessed += settings.channels;	
	}
	SDL_UnlockAudioDevice(audioDevice);
	
	spectrumPainter->feedWithInput(input);
	SDL_BlitSurface(imageSurface, NULL, screenSurface, NULL);
}


void RTSpectrumApp::drawLabels()
{	
	string text = string("Last ") + toString(settings.timeResolution * screenWidth) + " sec, ";
	text += string("0 - ") + toString(settings.freqResolution * screenHeight) + " Hz";
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

void RTSpectrumApp::clearRecording()
{
	SDL_LockAudioDevice(audioDevice);
	audioProcessed = 0;
	audioData.clear();
	SDL_UnlockAudioDevice(audioDevice);

	spectrumPainter->reset();	
}


void RTSpectrumApp::saveAudioRecording(const string &filename)
{
	cout << "Saving audio recording: " << filename << endl;
	
	SF_INFO sf_info;
	sf_info.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
	sf_info.samplerate = settings.sampleRate;
	sf_info.channels = settings.channels;
	SNDFILE *sf = sf_open(filename.c_str(), SFM_WRITE, &sf_info);
	Error::raiseIfNull(sf, "sf_open failed");

	for(int i = 0; i < audioData.size(); i += settings.channels)
		sf_writef_short(sf, &audioData[i], 1);
	sf_close(sf);
}


void RTSpectrumApp::saveSpectrumImage(const string &filename)
{
	cout << "Saving spectrum image: " << filename << endl;

	SDL_Surface *image = SpectrumPainter::audioToImage(audioData, settings);
	IMG_SavePNG(image, filename.c_str());	
	SDL_FreeSurface(image);
}


string RTSpectrumApp::timeString(time_t t)
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


int main(int argc, char **argv)
{
	try	{
		RTSpectrumApp app;
		app.run();
	}
	catch(Error e) {
		cout << "Error: " << e.getMessage() << endl;
		return 1;
	}
	return 0;
}
