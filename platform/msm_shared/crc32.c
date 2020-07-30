/* Copyright (c) 2015, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <debug.h>

static
const uint32_t crc32_table[256] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};

uint32_t crc32(uint32_t crc, const void *buf, size_t size)
{
	const uint8_t *p = buf;

	while (size--)
		crc = crc32_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	return crc;
}

#if 1 //def  QUECTEL_SYSTEM_BACKUP    // Ramos add for quectel 

#define CRC_TAB_SIZE    256             /* 2^CRC_TAB_BITS      */

/** Seed value for a 32-bit CRC. 
*/
#define CRC_32_SEED             0x00000000UL

/** Mask for a 32-bit CRC polynomial:

  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x^1+1

  @note1hang The x^32 tap is left off; it is implicit.
*/
#define CRC_32_POLYNOMIAL       0x04c11db7UL

/** Most significant bitmask for a CRC.
*/
#define CRC_32_MSB_MASK         0x80000000UL

/* CRC table for 32 bit CRC with generator polynomial 0x04c11db7,
** calculated 8 bits at a time, MSB first.
*/
const uint32 Q_crc32_table[ CRC_TAB_SIZE ] = {  
  0x00000000UL,  0x04c11db7UL,  0x09823b6eUL,  0x0d4326d9UL,
  0x130476dcUL,  0x17c56b6bUL,  0x1a864db2UL,  0x1e475005UL,
  0x2608edb8UL,  0x22c9f00fUL,  0x2f8ad6d6UL,  0x2b4bcb61UL,
  0x350c9b64UL,  0x31cd86d3UL,  0x3c8ea00aUL,  0x384fbdbdUL,
  0x4c11db70UL,  0x48d0c6c7UL,  0x4593e01eUL,  0x4152fda9UL,
  0x5f15adacUL,  0x5bd4b01bUL,  0x569796c2UL,  0x52568b75UL,
  0x6a1936c8UL,  0x6ed82b7fUL,  0x639b0da6UL,  0x675a1011UL,
  0x791d4014UL,  0x7ddc5da3UL,  0x709f7b7aUL,  0x745e66cdUL,
  0x9823b6e0UL,  0x9ce2ab57UL,  0x91a18d8eUL,  0x95609039UL,
  0x8b27c03cUL,  0x8fe6dd8bUL,  0x82a5fb52UL,  0x8664e6e5UL,
  0xbe2b5b58UL,  0xbaea46efUL,  0xb7a96036UL,  0xb3687d81UL,
  0xad2f2d84UL,  0xa9ee3033UL,  0xa4ad16eaUL,  0xa06c0b5dUL,
  0xd4326d90UL,  0xd0f37027UL,  0xddb056feUL,  0xd9714b49UL,
  0xc7361b4cUL,  0xc3f706fbUL,  0xceb42022UL,  0xca753d95UL,
  0xf23a8028UL,  0xf6fb9d9fUL,  0xfbb8bb46UL,  0xff79a6f1UL,
  0xe13ef6f4UL,  0xe5ffeb43UL,  0xe8bccd9aUL,  0xec7dd02dUL,
  0x34867077UL,  0x30476dc0UL,  0x3d044b19UL,  0x39c556aeUL,
  0x278206abUL,  0x23431b1cUL,  0x2e003dc5UL,  0x2ac12072UL,
  0x128e9dcfUL,  0x164f8078UL,  0x1b0ca6a1UL,  0x1fcdbb16UL,
  0x018aeb13UL,  0x054bf6a4UL,  0x0808d07dUL,  0x0cc9cdcaUL,
  0x7897ab07UL,  0x7c56b6b0UL,  0x71159069UL,  0x75d48ddeUL,
  0x6b93dddbUL,  0x6f52c06cUL,  0x6211e6b5UL,  0x66d0fb02UL,
  0x5e9f46bfUL,  0x5a5e5b08UL,  0x571d7dd1UL,  0x53dc6066UL,
  0x4d9b3063UL,  0x495a2dd4UL,  0x44190b0dUL,  0x40d816baUL,
  0xaca5c697UL,  0xa864db20UL,  0xa527fdf9UL,  0xa1e6e04eUL,
  0xbfa1b04bUL,  0xbb60adfcUL,  0xb6238b25UL,  0xb2e29692UL,
  0x8aad2b2fUL,  0x8e6c3698UL,  0x832f1041UL,  0x87ee0df6UL,
  0x99a95df3UL,  0x9d684044UL,  0x902b669dUL,  0x94ea7b2aUL,
  0xe0b41de7UL,  0xe4750050UL,  0xe9362689UL,  0xedf73b3eUL,
  0xf3b06b3bUL,  0xf771768cUL,  0xfa325055UL,  0xfef34de2UL,
  0xc6bcf05fUL,  0xc27dede8UL,  0xcf3ecb31UL,  0xcbffd686UL,
  0xd5b88683UL,  0xd1799b34UL,  0xdc3abdedUL,  0xd8fba05aUL,
  0x690ce0eeUL,  0x6dcdfd59UL,  0x608edb80UL,  0x644fc637UL,
  0x7a089632UL,  0x7ec98b85UL,  0x738aad5cUL,  0x774bb0ebUL,
  0x4f040d56UL,  0x4bc510e1UL,  0x46863638UL,  0x42472b8fUL,
  0x5c007b8aUL,  0x58c1663dUL,  0x558240e4UL,  0x51435d53UL,
  0x251d3b9eUL,  0x21dc2629UL,  0x2c9f00f0UL,  0x285e1d47UL,
  0x36194d42UL,  0x32d850f5UL,  0x3f9b762cUL,  0x3b5a6b9bUL,
  0x0315d626UL,  0x07d4cb91UL,  0x0a97ed48UL,  0x0e56f0ffUL,
  0x1011a0faUL,  0x14d0bd4dUL,  0x19939b94UL,  0x1d528623UL,
  0xf12f560eUL,  0xf5ee4bb9UL,  0xf8ad6d60UL,  0xfc6c70d7UL,
  0xe22b20d2UL,  0xe6ea3d65UL,  0xeba91bbcUL,  0xef68060bUL,
  0xd727bbb6UL,  0xd3e6a601UL,  0xdea580d8UL,  0xda649d6fUL,
  0xc423cd6aUL,  0xc0e2d0ddUL,  0xcda1f604UL,  0xc960ebb3UL,
  0xbd3e8d7eUL,  0xb9ff90c9UL,  0xb4bcb610UL,  0xb07daba7UL,
  0xae3afba2UL,  0xaafbe615UL,  0xa7b8c0ccUL,  0xa379dd7bUL,
  0x9b3660c6UL,  0x9ff77d71UL,  0x92b45ba8UL,  0x9675461fUL,
  0x8832161aUL,  0x8cf30badUL,  0x81b02d74UL,  0x857130c3UL,
  0x5d8a9099UL,  0x594b8d2eUL,  0x5408abf7UL,  0x50c9b640UL,
  0x4e8ee645UL,  0x4a4ffbf2UL,  0x470cdd2bUL,  0x43cdc09cUL,
  0x7b827d21UL,  0x7f436096UL,  0x7200464fUL,  0x76c15bf8UL,
  0x68860bfdUL,  0x6c47164aUL,  0x61043093UL,  0x65c52d24UL,
  0x119b4be9UL,  0x155a565eUL,  0x18197087UL,  0x1cd86d30UL,
  0x029f3d35UL,  0x065e2082UL,  0x0b1d065bUL,  0x0fdc1becUL,
  0x3793a651UL,  0x3352bbe6UL,  0x3e119d3fUL,  0x3ad08088UL,
  0x2497d08dUL,  0x2056cd3aUL,  0x2d15ebe3UL,  0x29d4f654UL,
  0xc5a92679UL,  0xc1683bceUL,  0xcc2b1d17UL,  0xc8ea00a0UL,
  0xd6ad50a5UL,  0xd26c4d12UL,  0xdf2f6bcbUL,  0xdbee767cUL,
  0xe3a1cbc1UL,  0xe760d676UL,  0xea23f0afUL,  0xeee2ed18UL,
  0xf0a5bd1dUL,  0xf464a0aaUL,  0xf9278673UL,  0xfde69bc4UL,
  0x89b8fd09UL,  0x8d79e0beUL,  0x803ac667UL,  0x84fbdbd0UL,
  0x9abc8bd5UL,  0x9e7d9662UL,  0x933eb0bbUL,  0x97ffad0cUL,
  0xafb010b1UL,  0xab710d06UL,  0xa6322bdfUL,  0xa2f33668UL,
  0xbcb4666dUL,  0xb8757bdaUL,  0xb5365d03UL,  0xb1f740b4UL
};


/*===========================================================================

FUNCTION CRC_32_CALC

DESCRIPTION
  This function calculates a 32-bit CRC over a specified number of data
  bits.  It can be used to produce a CRC and to check a CRC.  The CRC
  seed value can be used to compute a CRC over multiple (non-adjacent)
  chunks of data.

DEPENDENCIES
  None.

RETURN VALUE
  Returns a 32-bit word holding the contents of the CRC register as
  calculated over the specified data bits.

SIDE EFFECTS
  None.

===========================================================================*/
uint32 Q_crc_32_calc
(
  /* Pointer to data over which to compute CRC */
  uint8  *buf_ptr,

  /* Number of bits over which to compute CRC */
  uint16  len,

  /* Seed for CRC computation */
  uint32  seed
)
{
  /* Input data buffer */
  uint32  data;

  /* Working CRC buffer */
  uint32  crc;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ASSERT( buf_ptr != NULL );

  /*-------------------------------------------------------------------------
   Generate the CRC by looking up the transformation in a table and
   XOR-ing it into the CRC, one byte at a time.
  -------------------------------------------------------------------------*/
  for ( crc = seed; len >= 8; len -= 8, buf_ptr++ )
  {
    crc = Q_crc32_table[ ( ( crc >> 24 ) ^ ( *buf_ptr ) ) ] ^ ( crc << 8 );
  }

  /*-------------------------------------------------------------------------
   Finish calculating the CRC over the trailing data bits. This is done by
   aligning the remaining data bits with the CRC MSB, and then computing the
   CRC one bit at a time.
  -------------------------------------------------------------------------*/
  if ( len != 0 )
  {
    /*-----------------------------------------------------------------------
     Align data MSB with CRC MSB.
    -----------------------------------------------------------------------*/
    data = ( (uint32)( *buf_ptr ) ) << 24;

    /*-----------------------------------------------------------------------
     While there are bits left, compute CRC one bit at a time.
    -----------------------------------------------------------------------*/
    while (len-- != 0)
    {
      /*---------------------------------------------------------------------
       If the MSB of the XOR combination of the data and CRC is one, then
       advance the CRC register by one bit XOR with the CRC polynomial.
       Otherwise, just advance the CRC by one bit.
      ---------------------------------------------------------------------*/
      if ( ( ( crc ^ data ) & CRC_32_MSB_MASK ) != 0 )
      {
        crc <<= 1;
        crc ^= CRC_32_POLYNOMIAL;
      }      
      else
      {
        crc <<= 1;
      }

      /*---------------------------------------------------------------------
       Advance to the next input data bit and continue.
      ---------------------------------------------------------------------*/
      data <<= 1;
    }
  }

  /*-------------------------------------------------------------------------
   Return the resulting CRC value.
  -------------------------------------------------------------------------*/
  return( crc );

} /* crc_32_calc */

#endif

