#include "spectrumpainter.hpp"
#include <iostream>

void rdft(int n, int isgn, float *a);

SpectrumPainter::SpectrumPainter(SDL_Surface *imageSurface, const Settings &settings)
{
	this->settings = settings;
	this->imageSurface = imageSurface;
	window.resize(settings.fftSize);
	for(long i = 0; i < window.size(); ++i)
		window[i] = windowFunc(float(i) / window.size());
	reset();
}


void SpectrumPainter::feedWithInput(const vector<float> &input)
{
	for(int i = 0; i < input.size(); ++i)
	{
		block[blockPosition] = input[i];
		++blockPosition;
		++samplesProcessed;

		if(blockPosition == block.size())
		{
			vector<float> spectrum;
			frequencyAnalysis(block, spectrum);
			spectrums.push_back(spectrum);
			
			for(long i = 0; i < block.size() - settings.windowInc; ++i)
				block[i] = block[i + settings.windowInc];
			blockPosition -= settings.windowInc;
		}
	}

	drawSpectrogram(spectrums);
	spectrums.clear();
}

void SpectrumPainter::reset()
{
	blockPosition = 0;
	cursorPosition = 0;
	samplesProcessed = 0;
	scrolledTotal = 0;
	block.assign(settings.fftSize, 0);
	SDL_FillRect(imageSurface, NULL, SDL_MapRGB(imageSurface->format, 0, 0, 0));
}


void SpectrumPainter::drawSpectrogram(const vector< vector<float> > &spectrums)
{
	int move = cursorPosition - (imageSurface->w - spectrums.size());
	if(move > 0 && move < imageSurface->w)
	{
		SDL_Rect srcrect, dstrect;
		srcrect.x = move;
		srcrect.y = 0;
		srcrect.w = imageSurface->w - move;
		srcrect.h = imageSurface->h;
		dstrect.x = 0;
		dstrect.y = 0;			
		SDL_BlitSurface(imageSurface, &srcrect, imageSurface, &dstrect);
		cursorPosition = imageSurface->w - spectrums.size();
		scrolledTotal += move;
	}

	SDL_LockSurface(imageSurface);
	int xlimit = min(int(spectrums.size()), imageSurface->w - cursorPosition);
	for(int xpos = 0; xpos < xlimit; ++xpos)
		drawColumn(spectrums[xpos], cursorPosition + xpos);
	cursorPosition += spectrums.size();
	SDL_UnlockSurface(imageSurface);
}

void SpectrumPainter::drawColumn(const vector<float> &spectrum, int xpos)
{
	Uint8 *pixels = reinterpret_cast<Uint8*>(imageSurface->pixels);
	int ylimit = min(int(spectrum.size()), imageSurface->h);

	for(int ypos = 0; ypos < ylimit; ++ypos)
	{
		float amp = hypotf(spectrum[ypos * 2], spectrum[ypos * 2 + 1]); 
		float value = logarithmicScale(amp * sqrt(ypos) * settings.ampScale);
		setPixel32(imageSurface, xpos, imageSurface->h - ypos - 1,
			getColorSDL(imageSurface->format, value));
	}
}

void SpectrumPainter::frequencyAnalysis(const vector<float> &block, vector<float> &spectrum)
{
	spectrum.resize(block.size());
	for(int i = 0; i < block.size(); ++i)
		spectrum[i] = block[i] * window[i];
	rdft(block.size(), 1, &spectrum[0]);
	for(int i = 0; i < block.size(); ++i)
		spectrum[i] *= 2.0 / block.size();
}

float SpectrumPainter::windowFunc(float x)
{
	float tx = (2.0f * x - 1.0f) * settings.tradeoff;
	float y = expf(-powf(tx, 2.0f) * 0.5f) / sqrt(2.0f * M_PI) * sqrtf(settings.tradeoff) * 4.0f;
	return y * pow(sin(x * M_PI), 0.5);
}

float SpectrumPainter::logarithmicScale(float y)
{
	const float min = 1e-2;
	const float max = 1e-0f;
	return (logf(y + min) - logf(min)) / (logf(max) - logf(min));
}




void SpectrumPainter::getColor(float x, float &r, float &g, float &b)
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

Uint32 SpectrumPainter::getColorSDL(SDL_PixelFormat *format, float x)
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

void SpectrumPainter::setPixel32(SDL_Surface *surface, int x, int y, Uint32 color)
{
	if(x < 0 || x >= surface->w || y < 0 || y >= surface->h) {printf("Overflow: %d %d\n", x, y); exit(1);}
	Uint8* p = reinterpret_cast<Uint8*>(surface->pixels) + y * surface->pitch + x * surface->format->BytesPerPixel;
	p[0] = color & 0xff;
	p[1] = (color >> 8) & 0xff;
	p[2] = (color >> 16) & 0xff;
}




SDL_Surface* SpectrumPainter::audioToImage(const vector<Sint16> &audioData, const Settings &settings)
{
	int frames = audioData.size() / settings.channels;
	int imageWidth = (frames - settings.fftSize) / settings.windowInc + 1;
	int imageHeight = int(settings.upperFreqLimit / settings.freqResolution) + 1;
	if(imageHeight > settings.fftSize / 2) imageHeight = settings.fftSize / 2;
	if(imageWidth <= 0) imageWidth = 1;

	
	cout << "Frames: " << frames << endl;
	cout << "Length: " << float(frames) / settings.sampleRate << " sec" << endl;
	cout << "Frequency resolution: " << settings.freqResolution << " Hz" << endl;
	cout << "Time resolution: " << settings.timeResolution << " sec" << endl;


	cout << "Compute image of size " << imageWidth << "x" << imageHeight << ":" << endl;
	SDL_Surface *image = SDL_CreateRGBSurface(0, imageWidth, imageHeight, 24, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);
	Error::raiseIfNull(image, "SDL_CreateRGBSurface failed");

	SDL_LockSurface(image);

	SpectrumPainter spectrumPainter(image, settings);
	vector<float> input;
	
	for(int i = 0; i < frames; ++i)
	{	
		float monoSample = 0.0;
		for(int j = 0; j < settings.channels; ++j)
			monoSample += audioData[i * settings.channels + j];
		monoSample /= 32768.0f * settings.channels;
		input.push_back(monoSample);
	
		if(i % settings.sampleRate == 0)
		{
			cout << i / settings.sampleRate << " ";
			cout.flush();
			spectrumPainter.feedWithInput(input);
			input.clear();
		}
	}
	
	spectrumPainter.feedWithInput(input);
	cout << "Complete!" << endl;

	SDL_UnlockSurface(image);
	if(settings.labels) spectrumPainter.drawLabeling(image);
	
	return image;
}



void SpectrumPainter::drawLabeling(SDL_Surface *surface)
{
	const float frequencyGrid = 1000.0;
	const float timeGrid = 1.0;

	float timeStart = scrolledTotal * settings.timeResolution;
	float timeEnd = (scrolledTotal  + surface->w) * settings.timeResolution;
	
	int frequencySteps = ceil(settings.upperFreqLimit / frequencyGrid);	
	int timeStepsStart = floor(timeStart / timeGrid) - 1;
	int timeStepsEnd = ceil(timeEnd / timeGrid);
	SDL_Color textColor = { 255, 255, 255, 255 };

	for(int i = 1; i < frequencySteps; ++i) {
		string text = toString(i * frequencyGrid / 1000.0) + "kHz";

		SDL_Surface* textSurface = TTF_RenderText_Blended(settings.font, text.c_str(), textColor);
		Error::raiseIfNull(textSurface, "TTF_RenderText_Solid failed");
		
		SDL_Rect dstrect;
		dstrect.x = 0;
		dstrect.y = surface->h - i * frequencyGrid / settings.freqResolution - TTF_FontHeight(settings.font) / 2;
		SDL_BlitSurface(textSurface, NULL, surface, &dstrect);
		SDL_FreeSurface(textSurface);
	}

	for(int i = timeStepsStart; i <= timeStepsEnd; ++i) {
		string text = toString(i * timeGrid) + "s";
		
		SDL_Surface* textSurface = TTF_RenderText_Blended(settings.font, text.c_str(), textColor);
		Error::raiseIfNull(textSurface, "TTF_RenderText_Solid failed");
		
		SDL_Rect dstrect;		
		dstrect.x = i * timeGrid / settings.timeResolution - scrolledTotal;
		dstrect.y = surface->h - TTF_FontHeight(settings.font);				
		SDL_BlitSurface(textSurface, NULL, surface, &dstrect);
		SDL_FreeSurface(textSurface);
	}
}
