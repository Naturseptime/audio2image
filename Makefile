default: audio2image rtspectrum

audio2image: audio2image.cpp spectrumpainter.cpp spectrumpainter.hpp
	g++ fft4g_h_float.c audio2image.cpp  spectrumpainter.cpp -o audio2image -O2 `pkg-config SDL2_gfx --cflags --libs` `pkg-config SDL2_image --cflags --libs` `pkg-config sndfile --cflags --libs`
rtspectrum: rtspectrum.cpp spectrumpainter.cpp spectrumpainter.hpp
	g++ fft4g_h_float.c rtspectrum.cpp spectrumpainter.cpp -o rtspectrum -O2 `pkg-config SDL2_gfx --cflags --libs` `pkg-config SDL2_image --cflags --libs` `pkg-config SDL2_ttf --cflags --libs` `pkg-config sndfile --cflags --libs`

clean:
	rm audio2image rtspectrum
