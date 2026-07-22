/* lynx sprite packer
   written 97/02/17 42Bastian Schick

   input-formats :
   SPS  - ASCII file, every pixel represented by a hex-value
   color 0 also with SPACE.
   every line must end with LF ( or CR LF on !*nix-machines)
   RAW1 - a byte contains 8 pixels.
   RAW4 - a byte contains two pixels , each in one nibble.
   first nibble is the upper-left corner
   RAW8 - a byte is one pixel
   PCX  - 8 bit / 1plane ore 1bit/8plane type

   PI1  - 1 bit / 4 planes (ST-format)

   BMP - 4 or 8 bits per pixel, not RLE encoded

   DO   = Matthias Domin
   42BS = 42Bastian Schick (elw5basc@gp.fht-esslingen.de)
   CEF  = Carl Forhan (forhan@millcomm.com)

   last change :
   YY/MM/DD
   97/06/09

   97/03/20  DO    Support of SPS-files
   97/04/02  42BS  last-bit-bug
   added *nix case (no CR !)
   97/04/05  42BS  SPS: Last line may have LF (CR)
   added -v (verbose) and -c (color index compress)
   removed TABs from source
   output-file is now optional, default in+".spr"
   io separated, conversion of the hole file
   included PCX conversion.
   97/04/06  42BS  supports now 8bit/1plane or 1bit/4 plane PCX
   97/04/07  42BS  added palette output to the PCX-part
   also redirection for the SCB
   (needed with option -c !!)
   97/04/28  CEF   Added '!= 0' segment to eliminate compiler warning
   97/06/09  42BS  Added O_BINARY to IO.C (works now with DOS !)
   97/09/20  42BS  rebuild the interface ( xxxyyy )
   now with offset (-oxxxyyy)
   build in RAW1 for monochrome sources
   moved O_BINARY to sprpck.h
   added line-number to error-message

   97/11/25  42BS  moved a parameter-check to io.c
   changed default-type to PCX

   98/07/02  42BS  Started to add PI1-support
   98/07/02  42BS  finished PI1-support
   Input file need not to be reloaded => speed up
   98/07/23  42BS  Bug in ConvertPCX with 1 bit/8 planes PCX removed
   98/07/25  DO    MS Windows BMP-file support added
   98/08/01  DO    Added splitting of one picture into several sprites -ryyyxxx
   Auto-setting of sprite pixel size using -c _and_ -z
   98/08/07  42BS  Cleaned up BMP-loading and include bin2obj-stuff
   20/03/xx  42BS  Added 4bit/1plane PCX support
   Added Karri's "edgePen" idea
   Added literal compression.
   Code cleanup (more needed!)
*/

#define VER "2.5"
#include "sprpck.h"

int verbose;
BYTE rgb[32];
BYTE CollRedirect[16];

int dbg = 0;
int global_dbg = 0;

/* function-prototypes */
/* io.c */
extern void error( int line, char *w, ... );
extern void SaveRGB( char *filename, char *palname,  BYTE *data, int type, int size, int line );
extern void SaveSprite( char *filename, char *spritename, BYTE *data, int len, int line, int tzpe );
extern uint32_t LoadFile( char *filename, BYTE **adr );
extern long ConvertFile( BYTE *in, long in_size, int type,
                         int *in_w, int *in_h, int line );
extern BYTE* HandleOffset( BYTE * original,
                           int *in_w, int *in_h,
                           int off_x, int off_y,
                           int line );

/* sprpck.c */
void intobyte( int bits, BYTE val, BYTE **where );
BYTE * packline(const BYTE *in, BYTE *out, int len, int size);
BYTE * unpackline( BYTE *in, BYTE *out, int len, int size );
void   intobuffer( BYTE *in, BYTE *buf, int len, int size, BYTE *pColIndexes, int rev );
int    packit( BYTE *raw, int iw, /*int  ih,*/ BYTE **spr,
               BYTE size, int  packed,
               int  w, int h,
               int  act_x, int act_y,
               BYTE *pColIndexes,
               int edgePen );
int   CountColors( BYTE *raw, int iw, int w, int h, BYTE *pColIndexes );
int   get2val( char * s, int *a, int *b );

/* End Of Prototypes */

/*
  Count colors and compress color-index
  Done by Matthias Domin
*/

int CountColors( BYTE *raw, int iw, int w, int h, BYTE *pColIndexes )
{
  BYTE  buffer[514];         /* max. 512 pels/line */
  BYTE  bOrgColIndexes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  int   nColor[16];
  int   nCount, x, y;
  long  lSum;
  for ( x = 0; x < 16; ++x ) {     /* init. vectors */
    nColor[x] = 0;
    CollRedirect[x] = 0;
    *( pColIndexes+x ) = '-';
  }
  for ( y = 0 ; y < h ; ++y ) {
    intobuffer( raw+y*iw, buffer, w, 4, bOrgColIndexes, 0 );
    for ( x = 0; x < w; ++x ) {
      nColor[buffer[x]] += 1;
    }
  }
  nCount = 0;
  lSum = 0;
  for ( x = 0; x < 16; ++x ) {
    if ( nColor[x] )  {
      *( pColIndexes+x ) = nCount;
      CollRedirect[nCount] = x;           /* SCB-Index */
      ++nCount;
    }
    lSum += nColor[x];
    //if (verbose) printf("Color# %d is used %d times.\n", x, nColor[x]);
  }
  if ( verbose ) {
    for ( x = 0; x < 16; ++x ) {
      printf( "%4.1d|", x );
    }
    printf( "\n" );
    for ( x = 0; x < 16; ++x ) {
      printf( "----+" );
    }
    printf( "\n" );
    for ( x = 0; x < 16; ++x ) {
      printf( "%4.1d|", nColor[x] );
    }
    printf( "\n" );
    printf( "Pixel sum: %ld\n"
            "Colors used: %d\n"
            "Index table:\n", lSum, nCount );
    for ( x = 0; x < 16 ; ++x ) {
      if ( *( pColIndexes+x ) == '-' ) {
        printf( "-" );
      } else {
        printf( "%hX", *( pColIndexes+x ) );
      }
    }
    printf( "\n" );
  }
  return nCount;// DO, 1. Aug 1998: for auto-adjusting of # of sprite colors!
}

/*
  insert 'bits' bits from 'val' into a byte
  if a byte is complete, write it to 'where'
*/
void intobyte( int bits, BYTE val, BYTE **where )
{
  static BYTE bit_counter = 8, byte =0;
  BYTE *dst = *where;
  switch ( bits ) {
  case 0:
    /* init intobyte */
    bit_counter = 8;
    byte = 0;
    return;
  case 8:
    //->    if ( dbg && global_dbg ) printf("|%02x %d ",byte, bit_counter);
    /* handle end of line */
    *dst++ = byte;
    if ( byte & 0x1 ) {
      *dst++ = 0;    /* be sure the last bit is never set ! */
    }
    break;
  default:
    if ( bit_counter >= bits ) {
      val <<= ( bit_counter - bits );
      byte |= val;
      bit_counter -= bits;
      if ( bit_counter == 0 ) {
        *dst++ = byte;
        byte = 0;
        bit_counter = 8;
      }
    } else {
      byte |= val >> ( bits-bit_counter );
      val &= ( 1<<( bit_counter-bits ) )-1;
      *dst++ = byte;
      bit_counter = 8 - bits + bit_counter;
      byte = ( val << bit_counter );
    }
  }
  *where = dst;
}
void intobyte_reset()
{
  intobyte(0,0,NULL);
}

/*
 * packline() — RLE + literal packer for Lynx sprite scanlines.
 *
 * Input:  in   pixel array (len values, each in [0, 2^size-1])
 *         len  number of pixels (0 excluded by caller)
 *         size bits per pixel (1/2/3/4)
 * Output: packed byte stream starting at `out`, MSB-first, no padding.
 *         Returns pointer past the last written byte.
 *
 * Token kinds (each covers L pixels, 1<=L<=16):
 *   Packed : header(5 bits) = L-1       (bit4=0)
 *             value  (size bits)          repeated pixel value
 *             cost = 5 + size  (independent of L)
 *   Literal: header(5 bits) = 0x10|L-1  (bit4=1)
 *             L*size bits                  raw pixels in order
 *             cost = 5 + L*size
 *
 * Final byte count = floor(B/8)+1 where B = total bits across all tokens.
 * The +1 is unconditional (even when B%8==0, one extra zero byte appended).
 *
 * Algorithm: backward DP over pixel positions, minimising total bit count B.
 * Since payload_bytes = floor(B/8)+1, minimizing B is equivalent to minimizing
 * the output byte count.  dp[p] stores (total_bits >> 3, total_bits & 7) for
 * the optimal encoding of pixels [p .. len-1].
 */

#if 1 /* local LLM + human */
static int is_uniform(const BYTE *p, int L)
{
    BYTE v = *p;
    while (--L > 0) { if (*++p != v) return 0; }
    return 1;
}

/* packline                                                             */
/* ------------------------------------------------------------------ */
BYTE *packline(const BYTE *in, BYTE *out, int len, int size)
{
  /*
   * dp[p] = optimal total-bit cost to encode pixels [p .. len-1].
   * We use uint32_t for the bit count so the full path sum never overflows.
   * (worst case: 512 × (5+16*4) bits ≈ 34 KB — well within 32 bits.)
   */
  uint32_t dp[len + 1];

  /* Choice array: what to emit at position p with which run length. */
  struct { uint8_t tok_type; uint8_t L; } choice[len];
  /* tok_type: 0=packed, 1=literal */

  /* Base case: no pixels left => zero cost. */
  dp[len] = 0;

  /* Work backwards from the last pixel toward 0. */
  for (int p = len - 1; p >= 0; --p) {
    uint32_t best_bits = UINT32_MAX;

    for (uint8_t L = 1; L <= 16 && p + L <= len; ++L) {
      /* Packed token is valid only when all L pixels are identical. */
      int packed_cost = -1;
      if (is_uniform(in + p, L)){
        packed_cost = 5 + size;   /* 5+size bits, independent of L */
      }

      /* Literal token is always valid. */
      int lit_cost  = 5 + L * size;

      /* Pick the cheaper token type for this run length. */
      int cost      = lit_cost;           /* default: literal */
      uint8_t tok   = 1;                  /* default: literal */

      /* If lit_cost == packed_cost then choose literal ('00000' bug) */
      if (packed_cost >= 0 && packed_cost < lit_cost) {
        cost    = packed_cost;
        tok     = 0;                  /* packed wins (tie OK — same bit cost) */
      }

      /* Total bits via this candidate = token_bits + remainder encoded by dp[p+L]. */
      uint32_t total = (uint32_t)cost + dp[p + L];

      if (total < best_bits) {
        best_bits   = total;
        choice[p].tok_type = tok;
        choice[p].L        = L;
      }
    }
    dp[p] = best_bits;
  }

  /* Forward pass: emit tokens according to the recorded choices. */
  BYTE *dst  = out+1;

  intobyte(0, 0, &out);

  for (int p = 0; p < len; ) {
    int     L   = choice[p].L;
    uint8_t tok = choice[p].tok_type;

    if (tok == 0) {
      BYTE val = in[p];
      intobyte(5, (BYTE)(L - 1), &dst);
      intobyte(size, val , &dst);
    } else {
      intobyte(5, (BYTE)(0x10 | (L - 1)), &dst);
      for (int i = 0; i < L; ++i){
        intobyte(size, in[p + i] , &dst);
      }
    }
    p += L;  /* skip to next chunk */
  }
  /* End of line: flush partial byte via the terminator call. */
  intobyte(8, 0, &dst);
  *out = (BYTE)(dst-out);
  return dst;
}

#else /* Claude Sonnet 5 */
/*
  pack one line either to the left or right
*/
/*
  Optimaler Ersatz fuer packline() aus sprpck.c

  Ansatz: Shortest-Path-DP ueber die Pixelpositionen der Zeile.
  Kostenmodell (identisch zum Original-Bitstream-Format):

  Pack-Token    : 5 + size        Bit, deckt 1..16 identische Pixel
  Literal-Token : 5 + L*size      Bit, deckt 1..16 beliebige Pixel

  dp[i] = minimale Bitkosten zur Kodierung von in[0..i)
  Kanten von i nach i+L:
  - Literal, L = 1..min(16, len-i)                (immer moeglich)
  - Pack,    L = 1..(Laenge des konstanten Laufs)  (nur wenn in[i..i+L) konstant)

  Komplexitaet: O(16 * len) -> fuer len <= 512 trivial schnell.
  Liefert garantiert die bitminimale Token-Sequenz fuer dieses
  Format (kein Heuristik-Rest-Risiko, keine Rundenzahl mehr noetig).

  Voraussetzung: intobyte() wie im Original vorhanden/extern deklariert.
  "optimize" wird aus Kompatibilitaetsgruenden im Prototyp behalten,
  aber nicht mehr ausgewertet -- die DP ist immer optimal, ein
  Deaktivieren "spart" nichts mehr, sondern würde nur verschlechtern.
*/

#include <limits.h>

extern void intobyte( int bits, BYTE val, BYTE **where );

BYTE * packline(BYTE *in, BYTE *out, int len, int size )
{
  BYTE *out0 = out++;
  *out0 = 0;

  if ( len == 0 ) {
    return out0;
  }

  intobyte( 0, 0, &out );      /* init, wie im Original */

  /* 514 passend zu den vorhandenen buffer[514]-Deklarationen im Projekt */
  static int   dp[514];
  static short tok_len[514];
  static char  tok_packed[514];

  int i, L, maxL, cost, pl;

  dp[0] = 0;
  for ( i = 1; i <= len; ++i ) {
    dp[i] = INT_MAX;
  }

  for ( i = 0; i < len; ++i ) {
    if ( dp[i] == INT_MAX ) {
      continue;   /* unerreichbar -- kann bei diesem Kantenmodell nicht passieren,
                     bleibt als Sicherheitsnetz drin */
    }

    /* --- Literal-Kanten: Laenge 1..16, immer erlaubt --- */
    maxL = ( len - i < 16 ) ? ( len - i ) : 16;
    for ( L = 1; L <= maxL; ++L ) {
      cost = dp[i] + 5 + L * size;
      if ( cost < dp[i+L] ) {
        dp[i+L]         = cost;
        tok_len[i+L]     = (short)L;
        tok_packed[i+L]  = 0;
      }
    }

    /* --- Pack-Kanten: nur innerhalb eines konstanten Laufs --- */
    L = 1;
    while ( i + L < len && in[i+L] == in[i] && L < 16 ) {
      ++L;
    }
    for ( pl = 1; pl <= L; ++pl ) {
      cost = dp[i] + 5 + size;
      if ( cost < dp[i+pl] ) {
        dp[i+pl]         = cost;
        tok_len[i+pl]    = (short)pl;
        tok_packed[i+pl] = 1;
      }
    }
  }

  /* --- Backtracking: Kette der gewaehlten Tokens von len nach 0 --- */
  int pos_stack[514];
  int n = 0;
  for ( i = len; i > 0; i -= tok_len[i] ) {
    pos_stack[n++] = i;
  }

  /* --- Ausgabe in Vorwaertsreihenfolge, identischer Bitstream wie Original --- */
  int start = 0;
  for ( int k = n - 1; k >= 0; --k ) {
    int end = pos_stack[k];
    int l   = tok_len[end];

    if ( tok_packed[end] ) {
      intobyte( 5, l - 1, &out );
      intobyte( size, in[start], &out );
    } else {
      intobyte( 5, 0x10 | ( l - 1 ), &out );
      for ( int j = 0; j < l; ++j ) {
        intobyte( size, in[start+j], &out );
      }
    }
    start = end;
  }

  intobyte( 8, 0, &out );   /* Ende-Markierung, wie im Original */
  *out0 = ( BYTE )( out - out0 );
  return out;
}
#endif
/*
  pack the raw data into a literal sprite line
*/
BYTE * unpackline( BYTE *in, BYTE *out, int len, int size )
{
  BYTE *out0 = out++;
  intobyte( 0, 0, &out );
  while ( len ) {
    intobyte( size, *in, &out );
    ++in;
    --len;
  }
  intobyte( 8, 0, &out );
  *out0 = ( BYTE )( out - out0 );
  return out;
}
/*
  convert a line of raw data into a line of pixels
  reduced to the current bit-size
*/
void intobuffer( BYTE *in, BYTE *buf,
                 int len, int size, BYTE *pColIndexes, int reverse )
{
  BYTE b;
  int bit_mask = ( 1<<size )-1;
  int inc = 1;
  if ( reverse ) {
    buf += len-1;
    inc = -1;
  }
  while ( len ) {
    b = *in++;
    b = *( pColIndexes+b );           /* redirect Color Index */
    b &= bit_mask;
    *buf = b;
    buf += inc;
    --len;
  }
}

int findEdge( BYTE *buffer, int size_x, int edgePen )
{
  if ( edgePen >= 0 ) {
    buffer += size_x-1;
    while ( size_x > 1 && *buffer == edgePen ) {
      --size_x;
      --buffer;
    }
  }
  return size_x;
}
/*
  convert raw input into sprite data
*/
int packit( BYTE *raw, /* input data     */
            int  iw,
            /*         int  ih, */   /* input size     */
            BYTE **spr,   /* output data    */
            BYTE size,    /* bits per pixel */
            int  packed,  /* <>0 => pack it */
            int  w,
            int  h,       /* size of sprite */
            int  act_x,
            int  act_y,   /*   action point */
            BYTE  * pColIndexes,
            int edgePen )

{
  BYTE buffer[514];     /* max. 512 pels/line */
  BYTE *spr0;
  int y;
  int width;
  if ( ( *spr = spr0 = malloc( ( w+1 )*( h+1 ) ) ) == NULL ) {
    return 0;
  }
  /*** down/right ***/
  for ( y = act_y ; y < h ; ++y ) {
    width = w - act_x;
    intobuffer( raw+( y*iw )+act_x, buffer, width, size, pColIndexes, 0 );
    width = findEdge( buffer, width, edgePen );
    if ( packed ) {
      spr0 = packline( buffer, spr0, width, size );
    } else {
      spr0 = unpackline( buffer, spr0, width, size );
    }
  }
  /*** up/right ***/
  if ( act_y || act_x ) {
    *spr0++ = 0x01;
    for ( y = act_y-1 ; y >= 0 ; --y ) {
      width = w - act_x;
      intobuffer( raw+( y*iw )+act_x, buffer, width, size, pColIndexes, 0 );
      width = findEdge( buffer, width, edgePen );
      if ( packed ) {
        spr0 = packline( buffer, spr0, width, size);
      } else {
        spr0 = unpackline( buffer, spr0, width, size );
      }
    }
    /*** up/left ***/
    if ( act_x )  {
      *spr0++ = 0x01;
      for ( y = act_y-1 ; y >= 0  ; --y )    {
        width = act_x;
        intobuffer( raw+( y*iw ), buffer, width, size, pColIndexes, 1 );
        width = findEdge( buffer, width, edgePen );
        if ( packed ) {
          spr0 = packline( buffer, spr0, width, size);
        } else {
          spr0 = unpackline( buffer, spr0, width, size );
        }
      }
      /*** down/left ***/
      *spr0++ = 0x01;
      for ( y = act_y ; y < h ; ++y ) {
        width = act_x;
        intobuffer( raw+( y*iw ), buffer, width, size, pColIndexes, 1 );
        width = findEdge( buffer, width, edgePen );
        if ( packed ) {
          spr0 = packline( buffer, spr0, width, size);
        } else {
          spr0 = unpackline( buffer, spr0, width, size );
        }
      }
    }
  }
  *spr0++ = 0; /* end of sprite-data */
  return ( ( int )( spr0-*spr ) );
}

/* read two decimal values out of s */
/* format is : aaabbb               */

int get2val( char * s, int *a, int *b )
{
  if ( strlen( s ) != 6 ) {
    printf( "Error: Parameter must be 'xxxyyy' (decimal)!\n" );
    return ( 0 );
  }
  if ( ! sscanf( s+3, "%d", b ) ) {
    return ( 0 );
  }
  *( s+3 ) = 0;
  if ( ! sscanf( s, "%d", a ) ) {
    return ( 0 );
  }
  return ( 1 );
}

//remove any char not valid for a label in C or ASM
char *getCleanName(char *cleanname, char *filename)
{
  char *delim;

  //path delimiters
  strcpy( cleanname, filename );
  delim = strchr( cleanname, '/' );
  while ( delim != NULL ) {
    *delim = '_';
    delim = strchr( cleanname, '/' );
  }

  delim = strchr( cleanname, '\\' );
  while ( delim != NULL ) {
    *delim = '_';
    delim = strchr( cleanname, '/' );
  }

  //space in filename
  delim = strchr( cleanname, ' ' );
  while ( delim != NULL ) {
    *delim = '_';
    delim = strchr( cleanname, ' ' );
  }

  //remove the extension
  delim = strrchr( cleanname, '.' );
  if ( delim != NULL ) {
    *delim = 0;
    *( cleanname+( delim-cleanname ) ) = 0;
  }

  //now remove the . in path (directory or filename)
  delim = strchr( cleanname, '.' );
  while ( delim != NULL ) {
    *delim = '_';
    delim = strchr( cleanname, '.' );
  }

  return cleanname;
}

/****************************************************************/
#define CMD_OPT 14+1 /* command = argv[0] ! */

extern BYTE * original;
extern int org_w, org_h;

int main( int argc, char *argv[] )
{
  /* batch */
  FILE *batch_handle = NULL;
  char my_argv[CMD_OPT][32], cmdline[128];
  /* input */
  char *infile = "";
  BYTE *in, *raw;
  long in_size;
  int  in_w, in_h;
  int  t_x, t_y;
  /* output */
  char outfile[128];
  char outfile2[128];
  char palfile[128];
  char palname[128];
  char * extension;
  BYTE *out;
  int w, h, action_x, action_y, off_x, off_y, size, packed, type, sort_colindex, tiles, setsize;
  int pal_output;
  /* misc */
  int ret, i, val, val1, err;
  int line = 1;
  BYTE  bColIndexes[16];
  char *c_ptr, *dot;
  int t_xx, t_yy;
  int orgoff_x; // orgoff_y;
  int org_ww, org_hh;
  int nColorsUsed;
  int edgePen;
  /* end of var. decl. */
  global_dbg = 0;
  t_x = t_y = 1;
  if ( argc == 1 ) {
    printf( "-------------------------------\n"
            "Lynx Sprite Packer Ver "VER"\n"
            "(c) 1997..2021 42Bastian Schick\n"
            "               Matthias  Domin\n"
            "Contributions from\n"
            "               Karri Kaksonen\n"
            "               LordKraken\n"
            "-------------------------------\n" );
    printf( "Usage :\n"
            "sprpck [-c][-v][-O0][-e#][-s#][-t#][-u][-p#]\n"
            "[-axxxyyy][-Swwwhhh][-oxxxyyy][-iwwwhhh]\n"
            "[-rxxxyyy] [-z] in [out]\n"
            "or\n"
            "sprpck batchfile\n"
            "-e<pen>  : compress until color is only <pen>\n"
            "-c       : compress color index\n"
            "-v       : don't be quiet\n"
            "-s       : sprite-depth 4,3,2,1 bit(s) per pixel (4 default)\n"
            "-t       : type 0 = 4bit raw,  1 = 8bit raw, 2 = SPS, 3 = PCX (3 is default)\n"
            "           type 4 = 1bit raw type 5 = PI1 (Atari ST), 6 = MS Windows BMP \n"
            "-u       : unpacked     (packed is default)\n"
            "-p       : palette output-format : 0 - C, 1 - ASM, 2 - LYXASS(default)\n"
            "-axxxyyy : action point (e.g. -a200020)\n"
            "-Swwwhhh : sprite width and height (input-size is default)\n"
            "-oxxxyyy : offset in data (e.g. -o010200 )\n"
            "-iwwwhhh : input size (not needed for PCX)\n"
            "-rxxxyyy : split picture into yyy * xxx tiles\n"
            "-z       : (only possible with -c) auto-set sprite-depth \n"
            "in       : input data\n"
            "out      : output filename, optional, default is in.spr\n"
            "\n"
            "Note: With -p0 the sprite is saveed as cc65-Object file !\n\n"
            "In batchmode, lines must have the same format as in command\n"
            "line-mode, only if a input-file is defined in one line it can be\n"
            "omitted in the following lines.\n"
            "\n"
            "1bit raw =  8 pels per byte ( => -s1 is default )\n"
            "4bit raw => 2 pels per byte\n"
            "8bit raw => 1 pel  per byte\n"
            "SPS      => ASCII-hex-number per pel (blank = 0)\n"
            "PCX      => either 8 bits / 1 plane, 4bit / 1 plane or 1 bit / 4 planes\n"
            "PI1      => 1 bit / 4 planes , Atari ST Low Rez-format\n"
            "BMP      => either 8 bits or 4 bits not RLE encoded\n"
            );
    exit( 0 );
  }
  --argc;
  for ( i = 0; i < CMD_OPT ; ++i ) {
    my_argv[i][0] = 0;
  }
  if ( argc == 1 && ( batch_handle = fopen( argv[1], "r" ) ) == ( FILE * )NULL ) {
    error( 1, "Wrong or no batch file found :%s !\n", argv[1] );
  }
  do {
    /* set up default-values/switches */
    in_w     = 0;
    in_h     = 0;
    action_x = 0;
    action_y = 0;
    off_x    = 0;
    off_y    = 0;
    w        = 0;
    h        = 0;
    setsize  = 0;
    tiles    = 0;
    size     = 4;
    packed   = 1;
    type     = TYPE_PCX;
    sort_colindex = 0;
    pal_output = LYXASS_SRC;
    edgePen = -1;
#ifdef DEBUG
    verbose = 1;
#else
    verbose = 0;
#endif
    for ( i = 0 ; i<16 ; ++i ) {
      CollRedirect[i]= i;
      bColIndexes[i] = i;
    }
    /*
      clear arg-list
    */
    for ( i = 0; i < CMD_OPT ; ++i ) {
      my_argv[i][0] = 0;
    }
    if ( batch_handle ) {
      *cmdline = 0;
      fgets( cmdline, 127, batch_handle );
      argc = sscanf( cmdline, "%s %s %s %s %s %s %s %s %s %s %s",
                     my_argv[1], my_argv[2], my_argv[3], my_argv[4], my_argv[5],
                     my_argv[6], my_argv[7], my_argv[8], my_argv[9], my_argv[10],
                     my_argv[11] );
      // Check for comment line
      if ( my_argv[1][0] == ';' || my_argv[1][0] == '#' ) {
        continue;
      } else if ( argc <= 0 ) {
        break;
      }
    } else {
      for ( i = 1; i < CMD_OPT ; ++i )
        if ( argv[i] == NULL ) {
          break;
        } else {
          strcpy( my_argv[i], argv[i] );
        }
    }
    i = 1;
    c_ptr = my_argv[1];
    while ( *c_ptr == '-' && argc ) {
      switch ( *( c_ptr+1 ) ) {
      case 'e':
        err = sscanf( c_ptr+2, "%d", &val );
        if ( err ) {
          if ( val >= 0 && val <= 15 ) {
            edgePen = val;
            if ( verbose ) {
              printf( "Edge-Pen = %d\n", edgePen );
            }
          }
        }
        break;
      case 'r' :    // DO, 1. Aug. 1998
        if ( get2val( c_ptr+2, &val, &val1 ) ) {
          t_x = val;
          t_y = val1;
          if ( verbose ) {
            printf( "Split into (x*y) %d * %d tiles.\n", t_x, t_y );
          }
        }
        break;
      case 'z' :    // DO, 1. Aug. 1998
        setsize = 1;
        if ( verbose ) {
          printf( "Automatic setting of  sprite size !\n" );
        }
        break;
      case 's' :
        err = sscanf( c_ptr+2, "%d", &val );
        if ( err && val>0 && val <5 ) {
          if ( verbose ) {
            printf( "Set size to :%d\n", val );
          }
          size = val;
        }
        break;
      case 'u' :
        packed = 0;
        if ( verbose ) {
          printf( "Unpacked sprite !\n" );
        }
        break;
      case 't' :
        err = sscanf( c_ptr+2, "%d", &val );
        if ( err )   {
          if ( verbose ) {
            printf( "Setting Filetype :%d !\n", val );
          }
          type = val;    /* 0 = TYPE_RAW4, 1 = TYPE_RAW8, 2 = TYPE_SPS
                            3 = TYPE_PCX, 6 = TYPE_BMP */
          if ( type == TYPE_RAW1 )  {
            size = 1;
            if ( verbose ) {
              printf( "Set size to : 1\n" );
            }
          }
        }
        break;
      case 'v' :
        verbose = 1;
        break;
      case 'c' :
        sort_colindex = 1;
        break;
      case 'i' :
        if ( get2val( c_ptr+2, &val, &val1 ) ) {
          in_w = val;
          in_h = val1;
          if ( verbose ) {
            printf( "Setting input-size : %d,%d.\n", in_w, in_h );
          }
        }
        break;
      case 'a' :
        if ( get2val( c_ptr+2, &val, &val1 ) ) {
          action_x = val;
          action_y = val1;
          if ( verbose ) {
            printf( "Setting action-point :(%d,%d)\n", action_x, action_y );
          }
        }
        break;
      case 'o' :
        if ( get2val( c_ptr+2, &val, &val1 ) ) {
          off_x = val;
          off_y = val1;
          if ( verbose ) {
            printf( "Setting offset : (%d,%d)\n", off_x, off_y );
          }
        }
        break;
      case 'p' :
        err = sscanf( c_ptr+2, "%d", &val );
        if ( err )  {
          if ( verbose ) {
            printf( "Setting palette-output type :%d !\n", val );
          }
          pal_output = val;
        }
        break;
      case 'S' :
        if ( get2val( c_ptr+2, &val, &val1 ) ) {
          w = val;
          h = val1;
          if ( verbose ) {
            printf( "Setting sprite-size : %d,%d\n", w, h );
          }
        }
        break;
      case 'd':
        global_dbg = 1;
        break;
      default :
        error( line, "Unsupported option(s) ! Leaving ...\n" );
      }/*switch*/
      c_ptr=my_argv[++i];
      --argc;
    } /*while*/
    if ( *my_argv[i] == 0 && original == 0 ) {
      error( line, "Missing input-file !\n" );
    }
    //
    // get new input file if:
    //  a) two filenames are given
    //  b) one filename is given and no file was prev. loaded
    //
    if ( ( *my_argv[i] != 0 && *my_argv[i+1] != 0 )  ||
         ( *my_argv[i] != 0 && *my_argv[i+1] == 0 && !original )   ) {
      infile = my_argv[i++];
      if ( ! ( type == TYPE_PCX || type == TYPE_PI1 || type == TYPE_BMP )
           && ( ( in_w == 0 ) || ( in_h == 0 ) ) ) {
        error( line, "Input-size not set !\n" );
      }
      if ( ( in_size = LoadFile( infile, &in ) ) != 0 ) {
        if ( original ) {
          free( original );
        }
        in_size = ConvertFile( in, in_size, type, &in_w, &in_h, line );
        /* check input-filesize */
        if ( in_size != org_w * org_h ) {
          free( original );
          error( line, "Wrong picture-size (%d)\n", in_size );
        }
#ifdef DEBUG
        //SaveSprite( "raw.spr", original, in_size, line );
        printf( "w=%d, h=%d size=%ld\n", in_w, in_h, in_size );
#endif
      } else {
        error( line, "Couldn't load %s !\n", infile );
      }
    } else {
      in_w = org_w;
      in_h = org_h;
    }
    //
    // if no outfile if defined take the name of the input-file
    //
    if ( *my_argv[i] == 0 || !sscanf( my_argv[i], "%s", outfile ) ) {
      strcpy( outfile, infile );
    }
    //set it before adding extension
    getCleanName(palname, outfile);

    //
    // create output-filenames
    //
    strcpy( palfile, outfile );
    dot = strrchr( outfile, '.' );
    if ( dot != NULL ) {
      *dot = 0;
      *( palfile+( dot-outfile ) ) = 0;
    }
    strcat( palfile, ".pal" );
    if ( pal_output != C_HEADER ) {
      extension = ( char * )&".spr";
    } else {
      extension = ( char * )&".obj";
    }
    //
    //
    // if sprite-size is not defined, take input-size as default
    //
    if ( w == 0 ) {
      w = in_w;
    }
    if ( h == 0 ) {
      h = in_h;
    }
    if ( w > in_w || h > in_h )
      error( line, "Sprite > input picture !\nsprite = %d,%d input = %d,%d",
             w, h, in_w, in_h );
    tiles = t_x * t_y;
    if ( tiles )
      if ( ( off_x + t_x * w ) > in_w || ( off_y + t_y * h > in_h ) ) {
        error( line, "Can't split the picture in that many tiles !\n" );
      }
    //do  // Split the input file into several tiles ***************************
    //   {
    org_ww = org_w;
    org_hh = org_h;
    orgoff_x = off_x;
    //orgoff_y = off_y;
    for ( t_yy = 0; t_yy < t_y; t_yy++ ) {
      off_x = orgoff_x;
      for ( t_xx = 0; t_xx < t_x; t_xx++ )  {
        in_w = org_ww;
        in_h = org_hh;
        if ( verbose ) {
          printf( "Tile nr: %d\n", ( t_x * t_y ) - tiles + 1 );
        }
        //
        // do offset (original is preserved)
        //
        raw = HandleOffset( original, &in_w, &in_h, off_x, off_y, line );
        //
        // compress colors
        //
        if ( sort_colindex ) {
          nColorsUsed = CountColors( raw, in_w, w, h, bColIndexes );
#ifdef DEBUG
          printf ( "Colors %d\n", nColorsUsed );
#endif
          if ( setsize ) {
            if ( nColorsUsed <= 2 ) {
              size = 1;
            } else if ( nColorsUsed <= 4 ) {
              size = 2;
            } else if ( nColorsUsed <= 8 ) {
              size = 3;
            } else { // up to 16 colors
              size = 4;
            }
          }
        }
        //
        // now pack the sprite
        //
        ret = packit( raw, in_w,/*in_h,*/ &out, size, packed,
                      w, h, action_x, action_y, bColIndexes, edgePen );
        free( raw ); // no need for this anymore
        if ( ret ) {
          if ( tiles == 1 ) // Only once
            if ( type == TYPE_PCX || type == TYPE_PI1 || type == TYPE_BMP ) {
              SaveRGB( palfile, palname, rgb, pal_output, size, line );
            }
          // if just one tile, use the original outfile name
          if ( t_x * t_y == 1 ) {
            sprintf( outfile2, "%s%s", outfile, extension );
          } else {
            sprintf( outfile2, "%s%3.3d%3.3d%s", outfile, t_yy, t_xx, extension );
          }
          char spritename[128];
          getCleanName(spritename, outfile2);
          SaveSprite( outfile2, spritename, out, ret, line, pal_output );
          free( out );
        } else {
          error( line, "Packed size = 0!" );
        }
        off_x += w;  // next column of tiles;
        tiles--;
      } // for t_xx
      off_y += h; // next row of tiles
    } // for t_yy
    // } while (tiles); // split the input into several tiles *****************
    // do while is redundant to for x, y :)
  } while ( batch_handle && !feof( batch_handle ) );
  if ( batch_handle ) {
    fclose( batch_handle );
  }
#ifdef ATARI
  if ( verbose ) {
    getc( stdin );
  }
#endif
  return 0;
}
