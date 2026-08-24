// TCMIPS port: all sound stubbed out. No mixer, no AdLib, no digitized audio.
#include "wl_def.h"
#ifndef MIX_CHANNELS
#define MIX_CHANNELS 8
#endif

boolean         AdLibPresent = false;
boolean         SoundBlasterPresent = false;
boolean         SoundPositioned = false;
SDMode          SoundMode = sdm_Off;
SDSMode         DigiMode = sds_Off;
SMMode          MusicMode = smm_Off;
int             DigiMap[LASTSOUND];
int             DigiChannel[LASTSOUND];
globalsoundpos  channelSoundPos[MIX_CHANNELS];

void SD_Startup(void)
{
    for (int i = 0; i < LASTSOUND; i++)
    {
        DigiMap[i] = -1;
        DigiChannel[i] = -1;
    }
}

void SD_Shutdown(void) {}

int SD_GetChannelForDigi(int which) { (void)which; return 0; }

void SD_PositionSound(int leftvol, int rightvol)
{ (void)leftvol; (void)rightvol; }

boolean SD_PlaySound(soundnames sound) { (void)sound; return false; }

void SD_SetPosition(int channel, int leftvol, int rightvol)
{ (void)channel; (void)leftvol; (void)rightvol; }

void SD_StopSound(void) {}

void SD_WaitSoundDone(void) {}

void SD_StartMusic(int chunk) { (void)chunk; }

void SD_ContinueMusic(int chunk, int startoffs)
{ (void)chunk; (void)startoffs; }

void SD_MusicOn(void) {}

int SD_MusicOff(void) { return 0; }

void SD_FadeOutMusic(void) {}

boolean SD_MusicPlaying(void) { return false; }

boolean SD_SetSoundMode(SDMode mode) { (void)mode; return false; }

boolean SD_SetMusicMode(SMMode mode) { (void)mode; return false; }

word SD_SoundPlaying(void) { return false; }

void SD_SetDigiDevice(SDSMode mode) { (void)mode; }

void SD_PrepareSound(int which) { (void)which; }

int SD_PlayDigitized(word which, int leftpos, int rightpos)
{ (void)which; (void)leftpos; (void)rightpos; return 0; }

void SD_StopDigitized(void) {}
