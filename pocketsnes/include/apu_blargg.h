#ifdef USE_BLARGG_APU

#ifndef APU_BLARGG_H
#define APU_BLARGG_H

#include "snes9x.h"

typedef void (*dsp_copy_func_t)( uint8 ** io, void* state, size_t );

#define ECHO_HIST_SIZE    8
#define ECHO_HIST_SIZE_X2 16

/* Sound control */

/* Mutes voices corresponding to non-zero bits in mask (issues repeated KOFF events).
   Reduces emulation accuracy. */

#define VOICE_COUNT      8
#define EXTRA_SIZE       16
#define EXTRA_SIZE_DIV_2 8
#define BRR_BUF_SIZE     12
#define BRR_BUF_SIZE_X2  24
#define BRR_BLOCK_SIZE   9

/* DSP register addresses */

/* Global registers */

#define R_MVOLL 0x0C
#define R_MVOLR 0x1C
#define R_EVOLL 0x2C
#define R_EVOLR 0x3C
#define R_KON   0x4C
#define R_KOFF  0x5C
#define R_FLG   0x6C
#define R_ENDX  0x7C
#define R_EFB   0x0D
#define R_EON   0x4D
#define R_PMON  0x2D
#define R_NON   0x3D
#define R_DIR   0x5D
#define R_ESA   0x6D
#define R_EDL   0x7D
#define R_FIR   0x0F /* 8 coefficients at 0x0F, 0x1F ... 0x7F */

/* Voice registers */
#define V_VOLL   0x00
#define V_VOLR   0x01
#define V_PITCHL 0x02
#define V_PITCHH 0x03
#define V_SRCN   0x04
#define V_ADSR0  0x05
#define V_ADSR1  0x06
#define V_GAIN   0x07
#define V_ENVX   0x08
#define V_OUTX   0x09

/* Status flag handling */

/* Hex value in name to clarify code and bit shifting.
   Flag stored in indicated variable during emulation */

#define N80 0x80 /* nz */
#define V40 0x40 /* psw */
#define P20 0x20 /* dp */
#define B10 0x10 /* psw */
#define H08 0x08 /* psw */
#define I04 0x04 /* psw */
#define Z02 0x02 /* nz */
#define C01 0x01 /* c */

#define NZ_NEG_MASK 0x880	/* either bit set indicates N flag set */

#define REGISTER_COUNT 128

#define ENV_RELEASE 0
#define ENV_ATTACK  1
#define ENV_DECAY   2
#define ENV_SUSTAIN 3

typedef struct
{
   int32  buf [BRR_BUF_SIZE_X2]; /* decoded samples (twice the size to simplify wrap handling) */
   int32  buf_pos;               /* place in buffer where next samples will be decoded */
   int32  interp_pos;            /* relative fractional position in sample (0x1000 = 1.0) */
   int32  brr_addr;              /* address of current BRR block */
   int32  brr_offset;            /* current decoding offset in BRR block */
   uint8* regs;                  /* pointer to voice's DSP registers */
   int32  vbit;                  /* bitmask for voice: 0x01 for voice 0, 0x02 for voice 1, etc. */
   int32  kon_delay;             /* KON delay/current setup phase */
   int32  env_mode;
   int32  env;                   /* current envelope level */
   int32  hidden_env;            /* used by GAIN mode 7, very obscure quirk */
   uint8  t_envx_out;
} dsp_voice_t;

typedef struct
{
   uint8       regs [REGISTER_COUNT];
   int32       echo_hist [ECHO_HIST_SIZE_X2] [2]; /* Echo history keeps most recent 8 samples (twice the size to simplify wrap handling) */
   int32     (*echo_hist_pos) [2]; /* &echo_hist [0 to 7] */
   int32       every_other_sample; /* toggles every sample */
   int32       kon;                /* KON value when last checked */
   int32       noise;
   int32       counter;
   int32       echo_offset;        /* offset from ESA in echo buffer */
   int32       echo_length;        /* number of bytes that echo_offset will stop at */
   int32       phase; /* next clock cycle to run (0-31) */

   /* Hidden registers also written to when main register is written to */
   int32       new_kon;
   uint8       endx_buf;
   uint8       envx_buf;
   uint8       outx_buf;

   /* read once per sample */
   int32       t_pmon;
   int32       t_non;
   int32       t_eon;
   int32       t_dir;
   int32       t_koff;

   /* read a few clocks ahead then used */
   int32       t_brr_next_addr;
   int32       t_adsr0;
   int32       t_brr_header;
   int32       t_brr_byte;
   int32       t_srcn;
   int32       t_esa;
   int32       t_echo_enabled;

   /* internal state that is recalculated every sample */
   int32       t_dir_addr;
   int32       t_pitch;
   int32       t_output;
   int32       t_looped;
   int32       t_echo_ptr;

   /* left/right sums */
   int32       t_main_out [2];
   int32       t_echo_out [2];
   int32       t_echo_in  [2];

   dsp_voice_t voices [VOICE_COUNT];

   /* non-emulation state */
   uint8*      ram; /* 64K shared RAM between DSP and SMP */
   int16*      out;
   int16*      out_end;
   int16*      out_begin;
   int16       extra [EXTRA_SIZE];

   int32       rom_enabled;
   uint8*      rom;
   uint8*      hi_ram;
} dsp_state_t;

#if !SPC_NO_COPY_STATE_FUNCS

typedef struct
{
   dsp_copy_func_t func;
   uint8**         buf;
} spc_state_copy_t;

#define SPC_COPY( type, state ) state = (type) spc_copier_copy_int(&copier, state, sizeof (type) );

#endif

#define REG_COUNT   0x10
#define PORT_COUNT  4
#define TEMPO_UNIT  0x100
#define STATE_SIZE  68 * 1024L /* maximum space needed when saving */
#define TIMER_COUNT 3
#define ROM_SIZE    0x40
#define ROM_ADDR    0xFFC0

/* 1024000 SPC clocks per second, sample pair every 32 clocks */
#define CLOCKS_PER_SAMPLE 32

#define R_TEST     0x0
#define R_CONTROL  0x1
#define R_DSPADDR  0x2
#define R_DSPDATA  0x3
#define R_CPUIO0   0x4
#define R_CPUIO1   0x5
#define R_CPUIO2   0x6
#define R_CPUIO3   0x7
#define R_F8       0x8
#define R_F9       0x9
#define R_T0TARGET 0xA
#define R_T1TARGET 0xB
#define R_T2TARGET 0xC
#define R_T0OUT    0xD
#define R_T1OUT    0xE
#define R_T2OUT    0xF

/* Value that padding should be filled with */
#define CPU_PAD_FILL 0xFF

#if !SPC_NO_COPY_STATE_FUNCS
   /* Saves/loads state */
   void spc_copy_state( uint8 ** io, dsp_copy_func_t );
#endif

/* rel_time_t - Time relative to m_spc_time. Speeds up code a bit by eliminating
   need to constantly add m_spc_time to time from CPU. CPU uses time that ends
   at 0 to eliminate reloading end time every instruction. It pays off. */

typedef struct
{
   int32 next_time; /* time of next event */
   int32 prescaler;
   int32 period;
   int32 divider;
   int32 enabled;
   int32 counter;
} Timer;

/* Support SNES_MEMORY_APURAM */
uint8* spc_apuram();

typedef struct
{
   Timer    timers [TIMER_COUNT];
   uint8 smp_regs    [2] [REG_COUNT];

   struct
   {
      int32 pc;
      int32 a;
      int32 x;
      int32 y;
      int32 psw;
      int32 sp;
   } cpu_regs;

   int32  dsp_time;
   int32  spc_time;
   int32  tempo;
   int32  extra_clocks;
   int16* buf_begin;
   int16* buf_end;
   int16* extra_pos;
   int16  extra_buf   [EXTRA_SIZE];
   int32  rom_enabled;
   uint8  rom         [ROM_SIZE];
   uint8  hi_ram      [ROM_SIZE];
   uint8  cycle_table [256];

   struct
   {
      /* padding to neutralize address overflow */
      union
      {
         uint8  padding1 [0x100];
         uint16 align; /* makes compiler align data for 16-bit access */
      } padding1 [1];
      uint8 ram      [0x10000];
      uint8 padding2 [0x100];
   } ram;
} spc_state_t;

/* Number of samples written to output since last set */
#define SPC_SAMPLE_COUNT() ((m.extra_clocks >> 5) * 2)

typedef void (*apu_callback)();

#define SPC_SAVE_STATE_BLOCK_SIZE	(STATE_SIZE + 8)

bool    S9xInitAPU();
void    S9xDeinitAPU();
void    S9xResetAPU();
void    S9xSoftResetAPU();
uint8   S9xAPUReadPort(int32 port);
void    S9xAPUWritePort(int32 port, uint8 byte);
void    S9xAPUExecute();
void    S9xAPUSetReferenceTime(int32 cpucycles);
void    S9xAPUTimingSetSpeedup(int32 ticks);
void    S9xAPUAllowTimeOverflow(bool allow);
void    S9xAPULoadState(const uint8 * block);
void    S9xAPUSaveState(uint8 * block);

bool    S9xInitSound(int32 buffer_ms, int32 lag_ms);

bool    S9xSyncSound();
int32   S9xGetSampleCount();
void    S9xFinalizeSamples();
void    S9xClearSamples();
bool    S9xMixSamples(int16 * buffer, uint32 sample_count);
void    S9xSetSamplesAvailableCallback(apu_callback);

#endif /* APU_BLARGG_H */
#endif
