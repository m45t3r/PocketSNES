#include <stdlib.h>
#include <SDL.h>

#include "sal.h"

#define BUFFER_FRAMES 3
// 48000 Hz maximum; 1/50 of a second; 3 frames to hold (2 plus a bit extra)
#define BUFFER_SAMPLES (48000 / 50 * (BUFFER_FRAMES + 1))

extern void S9xMixSamples (u16 *buffer, int sample_count);

static SDL_AudioSpec audiospec;

volatile static unsigned int ReadPos, WritePos;

// 2 channels per sample (stereo); 2 bytes per sample-channel (16-bit)
static u16 Buffer[BUFFER_SAMPLES * 2 * 2];
static u32 SamplesPerFrame, BytesPerSample;
static u32 Muted; // S9xSetAudioMute(TRUE) gets undone after SNES Global Mute ends

static void sdl_audio_callback (void *userdata, Uint8 *stream, int len)
{
	u32 SamplesRequested = len / BytesPerSample, SamplesBuffered,
	    LocalWritePos = WritePos /* isolate a bit against races with the main thread */,
	    LocalReadPos = ReadPos /* keep a non-volatile copy at hand */;
	if (LocalReadPos <= LocalWritePos)
		SamplesBuffered = LocalWritePos - LocalReadPos;
	else
		SamplesBuffered = BUFFER_SAMPLES - (LocalReadPos - LocalWritePos);

	if (SamplesRequested > SamplesBuffered)
	{
		return;
	}

	if (Muted)
	{
		memset(stream, 0, len);
	}
	else if (LocalReadPos + SamplesRequested > BUFFER_SAMPLES)
	{
		memmove(stream, &Buffer[LocalReadPos * BytesPerSample], (BUFFER_SAMPLES - LocalReadPos) * BytesPerSample);
		memmove(&stream[(BUFFER_SAMPLES - LocalReadPos) * BytesPerSample], &Buffer[0], (SamplesRequested - (BUFFER_SAMPLES - LocalReadPos)) * BytesPerSample);
	}
	else
	{
		memmove(stream, &Buffer[LocalReadPos * BytesPerSample], len);
	}
	ReadPos = (LocalReadPos + SamplesRequested) % BUFFER_SAMPLES;
}

s32 sal_AudioInit(s32 rate, s32 bits, s32 Hz)
{
	int buffer = 1;
	audiospec.freq = rate;
	audiospec.channels = 2;
	audiospec.format = AUDIO_S16;
	audiospec.samples = (rate / Hz);
	audiospec.callback = sdl_audio_callback;

	SamplesPerFrame = audiospec.samples;
	BytesPerSample = audiospec.channels * (bits >> 3);

	//RS-97 fix, need to be power of 2
	while (buffer < audiospec.samples)
	{
		buffer*=2;
	}
	audiospec.samples = buffer;

	if (SDL_OpenAudio(&audiospec, NULL) < 0) {
		fprintf(stderr, "Unable to initialize audio.\n");
		return SAL_ERROR;
	}

	WritePos = ReadPos = 0;

	return SAL_OK;
}

void sal_AudioPause(void)
{
	SDL_PauseAudio(1);
}

void sal_AudioResume(void)
{
	SDL_PauseAudio(0);
}

void sal_AudioClose(void)
{
	SDL_CloseAudio();
}

void sal_AudioGenerate(u32 samples)
{
	u32 SamplesAvailable,
	    LocalReadPos = ReadPos /* isolate a bit against races with the audio thread */,
	    LocalWritePos = WritePos /* keep a non-volatile copy at hand */;
	if (LocalReadPos <= LocalWritePos)
		SamplesAvailable = BUFFER_SAMPLES - (LocalWritePos - LocalReadPos);
	else
		SamplesAvailable = LocalReadPos - LocalWritePos;
	if (samples >= SamplesAvailable)
	{
		samples = SamplesAvailable - 1;
	}
	if (samples > BUFFER_SAMPLES - LocalWritePos)
	{
		sal_AudioGenerate(BUFFER_SAMPLES - LocalWritePos);
		sal_AudioGenerate(samples - (BUFFER_SAMPLES - LocalWritePos));
	}
	else
	{
		S9xMixSamples(&Buffer[LocalWritePos * BytesPerSample], samples * audiospec.channels);
		WritePos = (LocalWritePos + samples) % BUFFER_SAMPLES;
	}
}

u32 sal_AudioGetFramesBuffered()
{
	u32 SamplesBuffered,
	    LocalReadPos = ReadPos /* isolate a bit against races with the audio thread */,
	    LocalWritePos = WritePos /* keep a non-volatile copy at hand */;
	if (LocalReadPos <= LocalWritePos)
		SamplesBuffered = LocalWritePos - LocalReadPos;
	else
		SamplesBuffered = BUFFER_SAMPLES - (LocalReadPos - LocalWritePos);
	return SamplesBuffered / SamplesPerFrame;
}

u32 sal_AudioGetMinFrames()
{
	return BUFFER_FRAMES - 1;
}

u32 sal_AudioGetMaxFrames()
{
	return BUFFER_FRAMES;
}

u32 sal_AudioGetSamplesPerFrame()
{
	return SamplesPerFrame;
}

u32 sal_AudioGetBytesPerSample()
{
	return BytesPerSample;
}

void sal_AudioSetVolume(s32 l, s32 r)
{
}

void sal_AudioSetMuted(u32 muted)
{
	Muted = muted;
}
