// ECEN5003, Project3, Module 2
// G.711 Encoder/Decoder

// WAV format reference:
//   http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
// G.711 reference:
//   https://en.wikipedia.org/wiki/G.711

#include<stdio.h>
#include<string.h>

#define WAV_FMT_PCM  1
#define WAV_FMT_ALAW 6
#define WAV_FMT_ULAW 7

// make sure our structs are packed for output
#ifdef __GNUC__
#define PACKED __attribute((packed))
#elif _MSC_VER
#pragma pack(1)
#define PACKED
#endif

typedef struct wav_header {
  char riff_str[4];
  unsigned int size;  // size of everything after this (filesize - 8 bytes)
  char wave_str[4];
} wav_header_t;

typedef struct wav_chunk {
  char id[4];
  unsigned int size;
} wav_chunk_t;

typedef struct fmt_chunk {
  unsigned short code;  // e.g. PCM
  unsigned short channels;
  unsigned int sample_rate; // blocks/s
  unsigned int data_rate;  // average bytes/s
  unsigned short block_size; // bytes/sample * channels
  unsigned short bits_per_sample;
} fmt_chunk_t;

typedef struct fact_chunk {
  unsigned int samples_per_channel;
} fact_chunk_t;


// separate, since format_size might force an offset
typedef struct data_chunk {
  char data_str[4];
  unsigned int data_size;
} wav_data_header_t;

typedef struct pcm_header {
  wav_header_t wav_header;
  wav_chunk_t fmt_header;
  fmt_chunk_t fmt;
  wav_chunk_t data_header;
} PACKED pcm_header_t;

typedef struct ulaw_header {
  wav_header_t wav_header;
  wav_chunk_t fmt_header;
  fmt_chunk_t fmt;
  short fmt_size_ext;
  wav_chunk_t fact_header;
  fact_chunk_t fact;
  wav_chunk_t data_header;
} PACKED ulaw_header_t;
//} __attribute((packed)) ulaw_header_t;

FILE *input;
FILE *output;

/* PCM <-> ulaw encoding scheme

   LPCM in (14b)      uLaw    LPCM out (14b)
   s00000001abcdx 	s000abcd 	s00000001abcd1
   s0000001abcdxx 	s001abcd 	s0000001abcd10
   s000001abcdxxx 	s010abcd 	s000001abcd100
   s00001abcdxxxx 	s011abcd 	s00001abcd1000
   s0001abcdxxxxx 	s100abcd 	s0001abcd10000
   s001abcdxxxxxx 	s101abcd 	s001abcd100000
   s01abcdxxxxxxx 	s110abcd 	s01abcd1000000
   s1abcdxxxxxxxx 	s111abcd 	s1abcd10000000
*/
// decode 8-bit ulaw into LPCM
// based on code from ...?
#define SIGN_MASK 0x80
#define EXP_MASK  0x70
#define MANT_MASK 0x0F
#define ULAW_BIAS 33
#define ULAW_CLIP 0x1FFF  /* 8191 */

// encode 16-bit PCM to 8-bit ulaw
char pcm_to_ulaw(short pcm) {
  char out;

  unsigned short s = 0;
  unsigned short e;
  unsigned short m;

  if(pcm < 0) {
    s = 1;
    pcm = -pcm;
  }

  pcm >>= 2; // ulaw expect 14-bit pcm, drop bottom bits
  pcm += ULAW_BIAS;
  // max/min PCM allowed is 13 bits of ones after sign
  if (pcm > ULAW_CLIP) { pcm = ULAW_CLIP; }
  //printf(" biased pcm: %04x\n", pcm);


  // find first non-zero position of input value, set e accordingly
  // input pcm is 16-bit value
  //   fedcba9876543210
  //   s00000001abcdxxx 	s010abcd  (bit 8 set, e=2)
  e = 7;  // start with maximum exponent, when 0x1000 (bit 13) is nonzero
  // test bit and work our way down until we hit a bit
  // NB: actual bit position is 5 more than e
  while( !(pcm & (1<<(e+5))) && (e>0) )  {
    e--;
  }

  // grab 4 lower bits after our first non-zero bit
  m = ( pcm & (0xF << (e+1)) );
  m >>= (e+1);
  //printf(" s: %x  e: %x  m: %x\n", s, e, m);

  out = (s << 7) | (e << 4) | m;
  //printf(" pre-flip: %hhx\n", out);

  // flip all bits per ulaw spec
  out = ~out;
  return out;
}


short ulaw_to_pcm(char ulaw) {
  // encoded bits: seeemmmm  (sign, exp, mantissa)
  // flip all bits, per ulaw spec
  unsigned char s;
  unsigned char e;
  unsigned char m;
  short out;
  ulaw = ~ulaw;
  s = (ulaw & SIGN_MASK) >> 7;
  e = (ulaw & EXP_MASK) >> 4;
  m = (ulaw & MANT_MASK);

  out = m;
  out |= 0x10;  // add leading 1 to mantissa (abcd -> 1abcd)
  out = out*2 + 1; // shift and add trailng 1 (1abcd -> 1abcd1)
  out <<= e;  // 1abcd1 * 2^e, shift according to exponent
  out -= ULAW_BIAS;  // remove bias, per spec
  out <<= 2;  // 16-bit PCM instead of 14-bit
  out = (s == 1) ? -out : +out;  // negate per encoded sign
  return out;
}


// Only need to decode ulaw for now
// Stub for future alaw decoder
short alaw_to_pcm(char alaw) {
  return 0;
}

void do_encode() {
  short in_block;
  char out_block;

  int i = 0 + sizeof(pcm_header_t);
  while( fread(&in_block, sizeof(in_block), 1, input) ) {
    out_block = pcm_to_ulaw(in_block);
    //printf( "%04x: 16-bit PCM %04hx", i, in_block);
    //printf( " <-> %02hhx 8-bit ulaw\n", out_block);
    fwrite(&out_block, sizeof(out_block), 1, output);
    i++;
  }
}

void do_decode(short fmt) {
  char in_block;
  short out_block;
  short (*decoder)(char);

  switch(fmt) {
    case WAV_FMT_ULAW:
      decoder = ulaw_to_pcm;
      break;
    case WAV_FMT_ALAW:
      decoder = alaw_to_pcm;
      break;
  }

  while( fread(&in_block, sizeof(in_block), 1, input) ) {
    out_block = decoder(in_block);
    //printf( "8-bit G.711 %02hhx", in_block);
    //printf( " <-> %04hx 16-bit ulaw\n", out_block);
    fwrite(&out_block, sizeof(out_block), 1, output);
  }
}


int main(int argc, char **argv) {

  wav_header_t wav_header;
  wav_chunk_t chunk;
  fmt_chunk_t fmt;
  unsigned int fact_samples_per_ch;
  unsigned int data_size;

  if (argc >= 2) {
    input = fopen(argv[1], "rb");
  } else {
    printf("Need filename!\n");
    return 1;
  }

  if( !input ) {
    printf("Can't open file: '%s'\n", argv[1]);
    return 1;
  }

  if( fread(&wav_header, sizeof(wav_header_t), 1, input) != 1 ) {
    printf("Could not read RIFF/WAV header!\n");
    return 1;
  }

  if( strncmp(wav_header.riff_str, "RIFF", 4) ) {
    printf("Not RIFF!\n");
    return 1;
  }
  if( strncmp(wav_header.wave_str, "WAVE", 4) ) {
    printf("Not WAV!\n");
    return 1;
  }



  do {

    // grab the next riff/wav chunk
    if( fread(&chunk, sizeof(wav_chunk_t), 1, input) != 1 ) {
      printf("Couldn't read next chunk!\n");
      return 1;
    } else {
      printf("Got chunk id '%.4s', size: %d\n", chunk.id, chunk.size);
    };

    if( !strncmp(chunk.id, "fmt ", 4) ) {
      if( !fread(&fmt, sizeof(fmt_chunk_t), 1, input) ) {
        printf("Couldn't read fmt chunk!\n");
        return 1;
      }
      printf(" Format: %u\n", fmt.code);
      printf(" Channels: %u\n", fmt.channels);
      printf(" Sample rate: %u blocks/s\n", fmt.sample_rate);
      printf(" Data rate: %u bytes/s\n", fmt.data_rate);
      printf(" Data block size: %u bytes\n", fmt.block_size);
      printf(" Bits/sample: %u\n", fmt.bits_per_sample);
      // skip fmt extension, if any
      if( chunk.size > sizeof(fmt_chunk_t) ) {
        if( fseek(input, chunk.size - sizeof(fmt_chunk_t), SEEK_CUR) ) {
          printf( "Failed to skip fmt extension!\n");
          return 1;
        };
      }
    }

    else if( !strncmp(chunk.id, "fact", 4) ) {
      if( !fread(&fact_samples_per_ch, sizeof(fact_samples_per_ch), 1, input) ) {
        printf("Couldn't read fact data!\n");
        return 1;
      }
      printf( " Fact: %d samples/ch\n", fact_samples_per_ch);
    }

    else if( !strncmp(chunk.id, "data", 4) ) {
      data_size = chunk.size;
    }

    else {
      printf( "Unknown chunk type!\n");
      return 1;
    }

  } while ( strncmp(chunk.id, "data", 4) );

  output = fopen("outfile.wav", "wb");
  switch( fmt.code ) {
    case WAV_FMT_PCM:
      // encode as as ulaw
      {
        ulaw_header_t header;
        memcpy(&header.wav_header, &wav_header, sizeof(wav_header));
        header.wav_header.size = sizeof(ulaw_header_t) + data_size/2;
        strncpy(header.fmt_header.id, "fmt ", 4);
        header.fmt_header.size = sizeof(fmt_chunk_t) + 2;  // 18
        header.fmt.code = WAV_FMT_ULAW;
        header.fmt.channels = fmt.channels; // stays same
        header.fmt.sample_rate = fmt.sample_rate; // stays same
        header.fmt.data_rate = fmt.sample_rate; // 8-bit samples
        header.fmt.block_size = 1;
        header.fmt.bits_per_sample = 8;
        header.fmt_size_ext = 0;
        strncpy(header.fact_header.id, "fact", 4);
        header.fact_header.size = sizeof(fact_chunk_t);
        header.fact.samples_per_channel = data_size / 2;
        strncpy(header.data_header.id, "data", 4);
        header.data_header.size = data_size / 2;
        fwrite(&header, sizeof(header), 1, output);
      }
      do_encode();
      break;
    case WAV_FMT_ULAW:
      // decode to pcm
      {
        pcm_header_t header;
        memcpy(&header.wav_header, &wav_header, sizeof(wav_header));
        header.wav_header.size = sizeof(pcm_header_t) + data_size*2;
        strncpy(header.fmt_header.id, "fmt ", 4);
        header.fmt_header.size = sizeof(fmt_chunk_t);
        header.fmt.code = WAV_FMT_PCM;
        header.fmt.channels = fmt.channels; // stays same
        header.fmt.sample_rate = fmt.sample_rate; // stays same
        header.fmt.data_rate = 2 * fmt.sample_rate; // 8-bit samples
        header.fmt.block_size = 2;
        header.fmt.bits_per_sample = 16;
        strncpy(header.data_header.id, "data", 4);
        header.data_header.size = data_size * 2;
        fwrite(&header, sizeof(header), 1, output);
      }
      do_decode(fmt.code);
      break;
    default:
      printf( "Unknown encoding!\n");
      return 1;
  }

  fclose(input);
  fclose(output);

  return 0;


}
