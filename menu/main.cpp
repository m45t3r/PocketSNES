
#include <sal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unzip.h"
#include "zip.h"
#include "menu.h"
#include "snes9x.h"
#include "memmap.h"
#include "apu.h"
#include "gfx.h"
#include "soundux.h"
#include "snapshot.h"
#include "scaler.h"

#define SNES_SCREEN_WIDTH  256
#define SNES_SCREEN_HEIGHT 192

#define FIXED_POINT 0x10000UL
#define FIXED_POINT_REMAINDER 0xffffUL
#define FIXED_POINT_SHIFT 16

static struct MENU_OPTIONS mMenuOptions;
static char mRomName[SAL_MAX_PATH]={""};

static s8 mFpsDisplay[16]={""};
static s8 mVolumeDisplay[16]={""};
static s8 mQuickStateDisplay[16]={""};
static u32 mFps=0;
static u32 mLastTimer=0;
static u32 mEnterMenu=0;
static u32 mQuickStateTimer=0;
static u32 mVolumeDisplayTimer=0;
static u32 mFramesCleared=0;
static u32 mInMenu=0;

static int S9xCompareSDD1IndexEntries (const void *p1, const void *p2)
{
    return (*(uint32 *) p1 - *(uint32 *) p2);
}

bool JustifierOffscreen (void)
{
	return true;
}

void JustifierButtons (uint32&)
{
}

void S9xProcessSound (unsigned int)
{
}

extern "C"
{

void S9xExit ()
{
}

u32 SamplesDoneThisFrame = 0;

void S9xGenerateSound (void)
{
	so.err_counter += so.err_rate;
	if ((Settings.SoundSync >= 2 && so.err_counter >= FIXED_POINT)
	 || (Settings.SoundSync == 1 && so.err_counter >= FIXED_POINT * 128))
	{
		u32 SamplesThisRun = so.err_counter >> FIXED_POINT_SHIFT;
		so.err_counter &= FIXED_POINT_REMAINDER;
		sal_AudioGenerate(SamplesThisRun);
		SamplesDoneThisFrame += SamplesThisRun;
	}
}

void S9xSetPalette ()
{

}

void S9xExtraUsage ()
{
}
	
void S9xParseArg (char **argv, int &index, int argc)
{	
}

bool8 S9xOpenSnapshotFile (const char *fname, bool8 read_only, STREAM *file)
{
	if (read_only)
	{
		if ((*file = OPEN_STREAM(fname,"rb")))
			return(TRUE);
	}
	else
	{
		if ((*file = OPEN_STREAM(fname,"w+b")))
			return(TRUE);
	}

	return (FALSE);	
}

const char* S9xGetSnapshotDirectory (void)
{
	return sal_DirectoryGetHome();
}
	
void S9xCloseSnapshotFile (STREAM file)
{
	CLOSE_STREAM(file);
}

void S9xMessage (int /* type */, int /* number */, const char *message)
{
	//MenuMessageBox("PocketSnes has encountered an error",(s8*)message,"",MENU_MESSAGE_BOX_MODE_PAUSE);
}

void erk (void)
{
      S9xMessage (0,0, "Erk!");
}

void S9xLoadSDD1Data (void)
{

}

u16 IntermediateScreen[SNES_WIDTH * SNES_HEIGHT_EXTENDED];
SDL_Surface *image;
extern SDL_Surface *mScreen;

bool LastPAL; /* Whether the last frame's height was 239 (true) or 224. */

bool8_32 S9xInitUpdate ()
{
	if (mInMenu) return TRUE;

#ifdef GCW_ZERO
	if (mMenuOptions.fullScreen == 3) GFX.Screen = (uint8*) mScreen->pixels;
	else 
#endif
	GFX.Screen = (u8*) IntermediateScreen; /* replacement needed after loading the saved states menu */
	
	return TRUE;
}

bool8_32 S9xDeinitUpdate (int Width, int Height, bool8_32)
{
	if(mInMenu) return TRUE;

	// After returning from the menu, clear the background of 3 frames.
	// This prevents remnants of the menu from appearing.
	if (mFramesCleared < 3)
	{
		sal_VideoClear(0);
		mFramesCleared++;
	}

	// If the height changed from 224 to 239, or from 239 to 224,
	// possibly change the resolution.
	bool PAL = !!(Memory.FillRAM[0x2133] & 4);
	if (PAL != LastPAL)
	{
		sal_VideoSetPAL(mMenuOptions.fullScreen, PAL);
		LastPAL = PAL;
	}

	switch (mMenuOptions.fullScreen)
	{
		case 0: /* No scaling */
		// case 3: /* Hardware scaling */
		{
			u32 h = PAL ? SNES_HEIGHT_EXTENDED : SNES_HEIGHT;
			u32 y, pitch = sal_VideoGetPitch();
			u8 *src = (u8*) IntermediateScreen, *dst = (u8*) sal_VideoGetBuffer()
				+ ((sal_VideoGetWidth() - SNES_WIDTH) / 2) * sizeof(u16)
				+ ((sal_VideoGetHeight() - h) / 2) * pitch;
			for (y = 0; y < h; y++)
			{
				memmove(dst, src, SNES_WIDTH * sizeof(u16));
				src += SNES_WIDTH * sizeof(u16);
				dst += pitch;
			}
			break;
		}
		case 1: /* Fast software scaling */
			if (PAL) {
				upscale_256x240_to_320x240((uint32_t*) sal_VideoGetBuffer(), (uint32_t*) IntermediateScreen, SNES_WIDTH);
			} else {
				upscale_256x224_to_320x240((uint32_t*) sal_VideoGetBuffer(), (uint32_t*) IntermediateScreen, SNES_WIDTH);
			}
			break;

		case 2: /* Smooth software scaling */
			if (PAL) {
				upscale_256x240_to_320x240_bilinearish((uint32_t*) sal_VideoGetBuffer() + 160, (uint32_t*) IntermediateScreen, SNES_WIDTH);
			} else {
				upscale_256x224_to_320x240_bilinearish((uint32_t*) sal_VideoGetBuffer() + 160, (uint32_t*) IntermediateScreen, SNES_WIDTH);
			}
			break;
	}

	u32 newTimer;
	if (mMenuOptions.showFps) 
	{
		mFps++;
		newTimer=sal_TimerRead();
		if(newTimer - mLastTimer > (u32)Memory.ROMFramesPerSecond)
		{
			mLastTimer=newTimer;
			sprintf(mFpsDisplay,"%2d/%2d", mFps, Memory.ROMFramesPerSecond);
			mFps=0;
		}
		
		sal_VideoDrawRect(0,0,5*8,8,SAL_RGB(0,0,0));
		sal_VideoPrint(0,0,mFpsDisplay,SAL_RGB(31,31,31));
	}

	if(mVolumeDisplayTimer>0)
	{
		sal_VideoDrawRect(100,0,8*8,8,SAL_RGB(0,0,0));
		sal_VideoPrint(100,0,mVolumeDisplay,SAL_RGB(31,31,31));
	}

	if(mQuickStateTimer>0)
	{
		sal_VideoDrawRect(200,0,8*8,8,SAL_RGB(0,0,0));
		sal_VideoPrint(200,0,mQuickStateDisplay,SAL_RGB(31,31,31));
	}

	sal_VideoFlip(0);

	return TRUE;
}

const char *S9xGetFilename (const char *ex)
{
	static char dir [SAL_MAX_PATH];
	char fname [SAL_MAX_PATH];
	char ext [SAL_MAX_PATH];

	sal_DirectorySplitFilename(Memory.ROMFilename, dir, fname, ext);
	strcpy(dir, sal_DirectoryGetHome());
	sal_DirectoryCombine(dir,fname);
	strcat (dir, ex);

	return (dir);
}

uint32 S9xReadJoypad (int which1)
{
	uint32 val=0x80000000;
	if (mInMenu) return val;
	if (which1 != 0) return val;

	u32 joy = sal_InputPoll();
	
	switch(mMenuOptions.menuKeystroke)
	{
		case 0:
        		if (((joy & SAL_INPUT_SELECT) && (joy & SAL_INPUT_START))
        		 || (joy & SAL_INPUT_MENU))
        		{
                		mEnterMenu = 1;
                		return val;
		        }
			break;
		case 1:
                        if (((joy & SAL_INPUT_L) && (joy & SAL_INPUT_R))
                         || (joy & SAL_INPUT_MENU))
                        {
                                mEnterMenu = 1;
                                return val;
                        }
			break;
		case 2:
                        if (((joy & SAL_INPUT_SELECT) && (joy & SAL_INPUT_START) && (joy & SAL_INPUT_L) && (joy & SAL_INPUT_R))
                         || (joy & SAL_INPUT_MENU))
                        {
                                mEnterMenu = 1;
                                return val;
                        }
			break;
		default:
                        if ((joy & SAL_INPUT_MENU))
                        {
                                mEnterMenu = 1;
                                return val;
                        }
			break;
	}
	
	/*if (((joy & SAL_INPUT_SELECT) && (joy & SAL_INPUT_START))
	 || (joy & SAL_INPUT_MENU))
	{
		mEnterMenu = 1;		
		return val;
	}*/

#if 0
	if ((joy & SAL_INPUT_L)&&(joy & SAL_INPUT_R)&&(joy & SAL_INPUT_UP))
	{
		if(mVolumeTimer==0)
		{
			mMenuOptions.volume++;
			if(mMenuOptions.volume>31) mMenuOptions.volume=31;
			sal_AudioSetVolume(mMenuOptions.volume,mMenuOptions.volume);
			mVolumeTimer=5;
			mVolumeDisplayTimer=60;
			sprintf(mVolumeDisplay,"Vol: %d",mMenuOptions.volume);
		}
		return val;
	}

	if ((joy & SAL_INPUT_L)&&(joy & SAL_INPUT_R)&&(joy & SAL_INPUT_DOWN))
	{
		if(mVolumeTimer==0)
		{
			mMenuOptions.volume--;
			if(mMenuOptions.volume>31) mMenuOptions.volume=0;
			sal_AudioSetVolume(mMenuOptions.volume,mMenuOptions.volume);
			mVolumeTimer=5;
			mVolumeDisplayTimer=60;
			sprintf(mVolumeDisplay,"Vol: %d",mMenuOptions.volume);
		}
		return val;
	}
#endif

	if (joy & SAL_INPUT_Y) val |= SNES_Y_MASK;
	if (joy & SAL_INPUT_A) val |= SNES_A_MASK;
	if (joy & SAL_INPUT_B) val |= SNES_B_MASK;
	if (joy & SAL_INPUT_X) val |= SNES_X_MASK;
		
	if (joy & SAL_INPUT_UP) 	val |= SNES_UP_MASK;
	if (joy & SAL_INPUT_DOWN) 	val |= SNES_DOWN_MASK;
	if (joy & SAL_INPUT_LEFT) 	val |= SNES_LEFT_MASK;
	if (joy & SAL_INPUT_RIGHT)	val |= SNES_RIGHT_MASK;
	if (joy & SAL_INPUT_START) 	val |= SNES_START_MASK;
	if (joy & SAL_INPUT_SELECT) 	val |= SNES_SELECT_MASK;
	if (joy & SAL_INPUT_L) 		val |= SNES_TL_MASK;
	if (joy & SAL_INPUT_R) 		val |= SNES_TR_MASK;

	return val;
}

bool8 S9xReadMousePosition (int /* which1 */, int &/* x */, int & /* y */,
		    uint32 & /* buttons */)
{
	S9xMessage (0,0,"read mouse");
	return (FALSE);
}

bool8 S9xReadSuperScopePosition (int & /* x */, int & /* y */,
				 uint32 & /* buttons */)
{
      S9xMessage (0,0,"read scope");
      return (FALSE);
}

const char *S9xGetFilenameInc (const char *e)
{
     S9xMessage (0,0,"get filename inc");
     return e;
}

#define MAX_AUDIO_FRAMESKIP 5

void S9xSyncSpeed(void)
{
	if (IsPreviewingState())
		return;

	if (Settings.SkipFrames == AUTO_FRAMERATE)
	{
		if (sal_AudioGetFramesBuffered() < sal_AudioGetMinFrames()
		 && ++IPPU.SkippedFrames < MAX_AUDIO_FRAMESKIP)
		{
			IPPU.RenderThisFrame = FALSE;
		}
		else
		{
			IPPU.RenderThisFrame = TRUE;
			IPPU.SkippedFrames = 0;
		}
	}
	else
	{
		if (++IPPU.SkippedFrames >= Settings.SkipFrames + 1)
		{
			IPPU.RenderThisFrame = TRUE;
			IPPU.SkippedFrames = 0;
		}
		else
		{
			IPPU.RenderThisFrame = FALSE;
		}
	}

	while (sal_AudioGetFramesBuffered() >= sal_AudioGetMaxFrames())
		usleep(1000);
}

const char *S9xBasename (const char *f)
{
      const char *p;

      S9xMessage (0,0,"s9x base name");

      if ((p = strrchr (f, '/')) != NULL || (p = strrchr (f, '\\')) != NULL)
         return (p + 1);

      return (f);
}

void PSNESForceSaveSRAM (void)
{
	if(mRomName[0] != 0)
	{
		Memory.SaveSRAM ((s8*)S9xGetFilename (".srm"));
	}
}

void S9xSaveSRAM (int showWarning)
{
	if (CPU.SRAMModified)
	{
		if(!Memory.SaveSRAM ((s8*)S9xGetFilename (".srm")))
		{
			MenuMessageBox("Saving SRAM","Failed!","",MENU_MESSAGE_BOX_MODE_PAUSE);
		}
	}
	else if(showWarning)
	{
		MenuMessageBox("SRAM saving ignored","No changes have been made to SRAM","",MENU_MESSAGE_BOX_MODE_MSG);
	}
}



}

bool8_32 S9xOpenSoundDevice(int a, unsigned char b, int c)
{
	return TRUE;
}

void S9xAutoSaveSRAM (void)
{
	if (mMenuOptions.autoSaveSram)
	{
		Memory.SaveSRAM (S9xGetFilename (".srm"));
		// sync(); // Only sync at exit or with a ROM change
	}
}

void S9xLoadSRAM (void)
{
	Memory.LoadSRAM ((s8*)S9xGetFilename (".srm"));
}

static u32 LastAudioRate = 0;
static u32 LastStereo = 0;
static u32 LastHz = 0;

static
int Run(int sound)
{
	bool PAL = !!(Memory.FillRAM[0x2133] & 4);

	sal_VideoEnterGame(mMenuOptions.fullScreen, PAL, Memory.ROMFramesPerSecond);
	LastPAL = PAL;

	Settings.SoundSync = mMenuOptions.soundSync;
	Settings.SkipFrames = mMenuOptions.frameSkip == 0 ? AUTO_FRAMERATE : mMenuOptions.frameSkip - 1;
	sal_TimerInit(Settings.FrameTime);

	if (sound) {
		/*
		Settings.SoundPlaybackRate = mMenuOptions.soundRate;
		Settings.Stereo = mMenuOptions.stereo ? TRUE : FALSE;
		*/
#ifndef FOREVER_16_BIT_SOUND
		Settings.SixteenBitSound=true;
#endif

		if (LastAudioRate != mMenuOptions.soundRate ||
			LastStereo != mMenuOptions.stereo ||
			LastHz != (u32)Memory.ROMFramesPerSecond)
		{
			if (LastAudioRate != 0)
			{
				sal_AudioClose();
			}
			sal_AudioInit(mMenuOptions.soundRate, 16,
						mMenuOptions.stereo, Memory.ROMFramesPerSecond);

			S9xInitSound (mMenuOptions.soundRate,
						mMenuOptions.stereo, sal_AudioGetSamplesPerFrame() * sal_AudioGetBytesPerSample());
			S9xSetPlaybackRate(mMenuOptions.soundRate);
			LastAudioRate = mMenuOptions.soundRate;
			LastStereo = mMenuOptions.stereo;
			LastHz = Memory.ROMFramesPerSecond;
		}
		sal_AudioSetMuted(0);

	} else {
		sal_AudioSetMuted(1);
	}
	sal_AudioResume();

  	while(!mEnterMenu) 
  	{
		//Run SNES for one glorious frame
		S9xMainLoop ();

		if (SamplesDoneThisFrame < sal_AudioGetSamplesPerFrame())
			sal_AudioGenerate(sal_AudioGetSamplesPerFrame() - SamplesDoneThisFrame);
		SamplesDoneThisFrame = 0;
		so.err_counter = 0;
  	}

	sal_AudioPause();

	sal_VideoExitGame();

	mEnterMenu=0;
	return mEnterMenu;

}

static inline int RunSound(void)
{
	return Run(1);
}

static inline int RunNoSound(void)
{
	return Run(0);
}

static 
int SnesRomLoad()
{
	MenuMessageBox("Loading ROM...",mRomName,"",MENU_MESSAGE_BOX_MODE_MSG);
	if (!Memory.LoadROM (mRomName))
	{
		MenuMessageBox("Loading ROM",mRomName,"Failed!",MENU_MESSAGE_BOX_MODE_PAUSE);
		return SAL_ERROR;
	}
	MenuMessageBox("Done loading the ROM",mRomName,"",MENU_MESSAGE_BOX_MODE_MSG);

	S9xReset();
	S9xResetSound(1);
	S9xLoadSRAM();
	return SAL_OK;
}

int SnesInit()
{
	ZeroMemory (&Settings, sizeof (Settings));

	Settings.JoystickEnabled = FALSE;
	Settings.SoundPlaybackRate = 44100;
	Settings.Stereo = TRUE;
	Settings.SoundBufferSize = 0;
	Settings.CyclesPercentage = 100;
	Settings.DisableSoundEcho = FALSE;
	Settings.APUEnabled = TRUE;
	Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
	Settings.SkipFrames = AUTO_FRAMERATE;
	Settings.Shutdown = Settings.ShutdownMaster = TRUE;
	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;
	Settings.FrameTime = Settings.FrameTimeNTSC;
	// Settings.DisableSampleCaching = FALSE;
	Settings.DisableMasterVolume = TRUE;
	Settings.Mouse = FALSE;
	Settings.SuperScope = FALSE;
	Settings.MultiPlayer5 = FALSE;
	//	Settings.ControllerOption = SNES_MULTIPLAYER5;
	Settings.ControllerOption = 0;

	Settings.InterpolatedSound = TRUE;
	Settings.StarfoxHack = TRUE;
	
	Settings.ForceTransparency = FALSE;
	Settings.Transparency = TRUE;
#ifndef FOREVER_16_BIT
	Settings.SixteenBit = TRUE;
#endif
	
	Settings.SupportHiRes = FALSE;
	Settings.NetPlay = FALSE;
	Settings.ServerName [0] = 0;
	Settings.AutoSaveDelay = 1;
	Settings.ApplyCheats = TRUE;
	Settings.TurboMode = FALSE;
	Settings.TurboSkipFrames = 15;
	Settings.ThreadSound = FALSE;
	Settings.SoundSync = 1;
	Settings.FixFrequency = TRUE;
	//Settings.NoPatch = true;		

	Settings.SuperFX = TRUE;
	Settings.DSP1Master = TRUE;
	Settings.SA1 = TRUE;
	Settings.C4 = TRUE;
	Settings.SDD1 = TRUE;

#ifdef GCW_ZERO
	if (mMenuOptions.fullScreen == 3) GFX.Screen = (uint8*) mScreen->pixels;
	else 
#endif
	GFX.Screen = (u8*) IntermediateScreen; /* replacement needed after loading the saved states menu */

	GFX.RealPitch = GFX.Pitch = 256 * sizeof(u16);
	
	GFX.SubScreen = (uint8 *)malloc(GFX.RealPitch * 480 * 2); 
	GFX.ZBuffer =  (uint8 *)malloc(GFX.RealPitch * 480 * 2); 
	GFX.SubZBuffer = (uint8 *)malloc(GFX.RealPitch * 480 * 2);
	GFX.Delta = (GFX.SubScreen - GFX.Screen) >> 1;
	GFX.PPL = GFX.Pitch >> 1;
	GFX.PPLx2 = GFX.Pitch;
	GFX.ZPitch = GFX.Pitch >> 1;
	
	if (Settings.ForceNoTransparency)
         Settings.Transparency = FALSE;

#ifndef FOREVER_16_BIT
	if (Settings.Transparency)
         Settings.SixteenBit = TRUE;
#endif

	Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;

	if (!Memory.Init () || !S9xInitAPU())
	{
		S9xMessage (0,0,"Failed to init memory");
		return SAL_ERROR;
	}

	//S9xInitSound ();
	
	//S9xSetRenderPixelFormat (RGB565);
	S9xSetSoundMute (TRUE);

	if (!S9xGraphicsInit ())
	{
		S9xMessage (0,0,"Failed to init graphics");
		return SAL_ERROR;
	}

	return SAL_OK;
}

void _makepath (char *path, const char *, const char *dir,
	const char *fname, const char *ext)
{
	if (dir && *dir)
	{
		strcpy (path, dir);
		strcat (path, "/");
	}
	else
	*path = 0;
	strcat (path, fname);
	if (ext && *ext)
	{
		strcat (path, ".");
		strcat (path, ext);
	}
}

void _splitpath (const char *path, char *drive, char *dir, char *fname,
	char *ext)
{
	*drive = 0;

	char *slash = strrchr ((char*)path, '/');
	if (!slash)
		slash = strrchr ((char*)path, '\\');

	char *dot = strrchr ((char*)path, '.');

	if (dot && slash && dot < slash)
		dot = NULL;

	if (!slash)
	{
		strcpy (dir, "");
		strcpy (fname, path);
		if (dot)
		{
			*(fname + (dot - path)) = 0;
			strcpy (ext, dot + 1);
		}
		else
			strcpy (ext, "");
	}
	else
	{
		strcpy (dir, path);
		*(dir + (slash - path)) = 0;
		strcpy (fname, slash + 1);
		if (dot)
		{
			*(fname + (dot - slash) - 1) = 0;
			strcpy (ext, dot + 1);
		}
		else
			strcpy (ext, "");
	}
} 

extern "C"
{

int mainEntry(int argc, char* argv[])
{
	s32 event=EVENT_NONE;

	sal_Init();
	sal_VideoInit(16);

	mRomName[0]=0;
	if (argc >= 2) 
 		strcpy(mRomName, argv[1]); // Record ROM name

	MenuInit(sal_DirectoryGetHome(), &mMenuOptions);

	if(SnesInit() == SAL_ERROR)
	{
		sal_Reset();
		return 0;
	}

	while(1)
	{
		mInMenu=1;
		event=MenuRun(mRomName);
		mInMenu=0;

		if(event==EVENT_LOAD_ROM)
		{
			if (mRomName[0] != 0)
			{
				MenuMessageBox("Saving SRAM...","","",MENU_MESSAGE_BOX_MODE_MSG);
				PSNESForceSaveSRAM();
			}
			if(SnesRomLoad() == SAL_ERROR) 
			{
				mRomName[0] = 0;
				MenuMessageBox("Failed to load ROM",mRomName,"Press any button to continue", MENU_MESSAGE_BOX_MODE_PAUSE);
				sal_Reset();
		    		return 0;
			}
			else
			{
				MenuMessageBox("Loading game settings...",mRomName,"",MENU_MESSAGE_BOX_MODE_MSG);
				LoadCurrentOptions();

				event=EVENT_RUN_ROM;
		  	}
		}

		if(event==EVENT_RESET_ROM)
		{
			S9xReset();
			event=EVENT_RUN_ROM;
		}

		if(event==EVENT_RUN_ROM)
		{
			sal_AudioSetVolume(mMenuOptions.volume,mMenuOptions.volume);
			sal_CpuSpeedSet(mMenuOptions.cpuSpeed);	
			mFramesCleared = 0;
			if(mMenuOptions.soundEnabled) 	
				RunSound();
			else
				RunNoSound();

			event=EVENT_NONE;
		}

		if(event==EVENT_EXIT_APP) break;	
	}

	MenuMessageBox("Saving SRAM...","","",MENU_MESSAGE_BOX_MODE_MSG);
	PSNESForceSaveSRAM();
	
	if (Settings.SPC7110)
		Del7110Gfx();
	S9xGraphicsDeinit();
	S9xDeinitAPU();
	Memory.Deinit();

	free(GFX.SubZBuffer);
	free(GFX.ZBuffer);
	free(GFX.SubScreen);
	GFX.SubZBuffer=NULL;
	GFX.ZBuffer=NULL;
	GFX.SubScreen=NULL;

	sal_Reset();
  	return 0;
}

}
