#include "btfy_ffft.h"

#include "btfy_consts_gft.h"

#include "gfv_tower.h"

#include "gft_mul_vi.h"

#include "btfy_rvv.h"

static
void (* gft_mul_vi [12]) (uint32_t * x, const uint32_t * y);

#define SWAPMOVE_ASM(a,b,m,N,tmp)                             \
do {                                                          \
  (tmp) = (((b) ^ ((a) >> (N))) & (uint32_t)(m));          \
  (b) ^= (tmp);                                             \
  (a) ^= ((tmp) << (N));                                    \
} while (0)


static
void btfy_layer( uint32_t* data, int datalen , int si )
{
  int blk_size = 1<<(si+1);
  int len = 1<<si;
  int count = 0;
  uint32_t temp[32];
  uint32_t temp2[32];

  int k=0;
  {
    // count = 0;
    uint32_t *data_k = data+k;
    for(int i=0;i<len;i+=32) {
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_btf_xor32_1src(data_k+i, data_k+len+i, temp2);
    }
    k += blk_size;
    count++;
  }
  if(k<datalen) {
    // count = 1;
    uint32_t *data_k = data+k;
    for(int i=0;i<len;i+=32) {
      gf232v_mul_0x2(temp,&data_k[len+i]);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_btf_xor32_2src(data_k+i, data_k+len+i, temp, temp2);
    }
    k += blk_size;
    count++;
  }
  if(k<datalen) {
    // count = 2;
    uint32_t *data_k = data+k;
    for(int i=0;i<len;i+=32) {
      gf232v_mul_0x5(temp,&data_k[len+i]);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_btf_xor32_2src(data_k+i, data_k+len+i, temp, temp2);
    }
    k += blk_size;
    count++;
  }
  for(;k<datalen;k+=blk_size) {
    uint32_t si_a = s0_gft[count++];
    uint32_t *data_k = data+k;
    if( 16 > si_a )
    for(int i=0;i<len;i+=32) {
      gf232v_mulscalar_gf16(temp,&data_k[len+i],si_a);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_btf_xor32_2src(data_k+i, data_k+len+i, temp, temp2);
    }
    else // if (256 > si_a )
    for(int i=0;i<len;i+=32) {
      gf232v_mulscalar_gf256(temp,&data_k[len+i],si_a);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_btf_xor32_2src(data_k+i, data_k+len+i, temp, temp2);
    }
  }

}



///////////////////////////////

#include <stdio.h>

static
void btfy_layer_s4( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s4_gf16_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0000ffff, 16);
    gf232v_mul_gf16( tmp , data+i+32 , ptr+(pi<<2) );
    (*gft_mul_vi[4])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    if( (++pi) >= S4_GF16_NUMX32 ) break;
  }

  ptr = s4_gf256_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0000ffff, 16);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[4])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    ++pi;
  }
}



static
void btfy_layer_s3( uint32_t*data, int datalen )
{
	uint32_t tmp[32];
	uint32_t tmp2[32];

	const uint32_t *ptr;
	int pi;

	int i = 0;
	ptr = s3_gf256_vec;
	pi = 0;
	while (i < datalen) {
		// reorder data (Iter 3: RVV SWAPMOVE)
		RVV_SWAPMOVE32(data+i, data+i+32, 0x00ff00ff, 8);
		gf232v_mul_gf256(tmp, data + i + 32, ptr + (pi << 3));
		(*gft_mul_vi[3])(tmp2, data + i + 32);
		rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
		i += 64;
		if ((++pi) >= S3_GF256_NUMX32) break;
	}

	ptr = s3_gf216_vec;
	pi = 0;
	while (i < datalen) {
		// reorder data (Iter 3: RVV SWAPMOVE)
		RVV_SWAPMOVE32(data+i, data+i+32, 0x00ff00ff, 8);
		gf232v_mul_gf216(tmp, data + i + 32, ptr + (pi << 4));
		(*gft_mul_vi[3])(tmp2, data + i + 32);
		rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
		i += 64;
		++pi;
	}
}



static
void btfy_layer_s2( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s2_gf256_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0f0f0f0f, 4);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[2])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    if( (++pi) >= S2_GF256_NUMX32 ) break;
  }

  ptr = s2_gf216_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0f0f0f0f, 4);
    gf232v_mul_gf216( tmp , data+i+32 , ptr+(pi<<4) );
    (*gft_mul_vi[2])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    ++pi;
  }
}




static
void btfy_layer_s1( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s1_gf256_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x33333333, 2);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[1])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    if( (++pi) >= S1_GF256_NUMX32 ) break;
  }

  ptr = s1_gf216_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x33333333, 2);
    gf232v_mul_gf216( tmp , data+i+32 , ptr+(pi<<4) );
    (*gft_mul_vi[1])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    ++pi;
  }
}




static
void btfy_layer_s0( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s0_gf256_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x55555555, 1);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[0])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    if( (++pi) >= S0_GF256_NUMX32 ) break;
  }

  ptr = s0_gf216_vec;
  pi = 0;
  while(i<datalen) {
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x55555555, 1);
    gf232v_mul_gf216( tmp , data+i+32 , ptr+(pi<<4) );
    (*gft_mul_vi[0])( tmp2 , data+i+32 );
    rvv_btf_xor32_2src(data+i, data+i+32, tmp, tmp2);
    i+=64;
    ++pi;
  }
}




//////////////////////////////////////////////////


//void (* gft_mul_vi [12]) (uint32_t * x, const uint32_t * y);


void btfy_32768( uint32_t * poly )
{
  int datalen = 1024;

  gft_mul_vi[0] = gft_mul_v26;
  gft_mul_vi[1] = gft_mul_v25;
  gft_mul_vi[2] = gft_mul_v24;
  gft_mul_vi[3] = gft_mul_v23;
  gft_mul_vi[4] = gft_mul_v22;
  gft_mul_vi[5] = gft_mul_v21;
  gft_mul_vi[6] = gft_mul_v20;
  gft_mul_vi[7] = gft_mul_v19;
  gft_mul_vi[8] = gft_mul_v18;
  gft_mul_vi[9] = gft_mul_v17;

  btfy_layer( poly , datalen , 9 );
  btfy_layer( poly , datalen , 8 );
  btfy_layer( poly , datalen , 7 );
  btfy_layer( poly , datalen , 6 );
  btfy_layer( poly , datalen , 5 );

  btfy_layer_s4( poly , datalen );
  btfy_layer_s3( poly , datalen );
  btfy_layer_s2( poly , datalen );
  btfy_layer_s1( poly , datalen );
  btfy_layer_s0( poly , datalen );
}

void btfy_65536( uint32_t * poly )
{
  int datalen = 2048;

  gft_mul_vi[0] = gft_mul_v27;
  gft_mul_vi[1] = gft_mul_v26;
  gft_mul_vi[2] = gft_mul_v25;
  gft_mul_vi[3] = gft_mul_v24;
  gft_mul_vi[4] = gft_mul_v23;
  gft_mul_vi[5] = gft_mul_v22;
  gft_mul_vi[6] = gft_mul_v21;
  gft_mul_vi[7] = gft_mul_v20;
  gft_mul_vi[8] = gft_mul_v19;
  gft_mul_vi[9] = gft_mul_v18;
  gft_mul_vi[10] = gft_mul_v17;

  btfy_layer( poly , datalen , 10 );
  btfy_layer( poly , datalen , 9 );
  btfy_layer( poly , datalen , 8 );
  btfy_layer( poly , datalen , 7 );
  btfy_layer( poly , datalen , 6 );
  btfy_layer( poly , datalen , 5 );

  btfy_layer_s4( poly , datalen );
  btfy_layer_s3( poly , datalen );
  btfy_layer_s2( poly , datalen );
  btfy_layer_s1( poly , datalen );
  btfy_layer_s0( poly , datalen );
}



//new code block
void btfy_131072(uint32_t * poly)
{
	int datalen = 4096;

	gft_mul_vi[0] = gft_mul_v28;
	gft_mul_vi[1] = gft_mul_v27;
	gft_mul_vi[2] = gft_mul_v26;
	gft_mul_vi[3] = gft_mul_v25;
	gft_mul_vi[4] = gft_mul_v24;
	gft_mul_vi[5] = gft_mul_v23;
	gft_mul_vi[6] = gft_mul_v22;
	gft_mul_vi[7] = gft_mul_v21;
	gft_mul_vi[8] = gft_mul_v20;
	gft_mul_vi[9] = gft_mul_v19;
	gft_mul_vi[10] = gft_mul_v18;
	gft_mul_vi[11] = gft_mul_v17;

	btfy_layer(poly, datalen, 11);
	btfy_layer(poly, datalen, 10);
	btfy_layer(poly, datalen, 9);
	btfy_layer(poly, datalen, 8);
	btfy_layer(poly, datalen, 7);
	btfy_layer(poly, datalen, 6);
	btfy_layer(poly, datalen, 5);

	btfy_layer_s4(poly, datalen);
	btfy_layer_s3(poly, datalen);
	btfy_layer_s2(poly, datalen);
	btfy_layer_s1(poly, datalen);
	btfy_layer_s0(poly, datalen);
}



//////////////////////////////////////////////////



static
void ibtfy_layer( uint32_t* data, int datalen , int si )
{
  int blk_size = 1<<(si+1);
  int len = 1<<si;
  int count = 0;
  uint32_t temp[32];
  uint32_t temp2[32];

  int k=0;
  {
    // count = 0;
    uint32_t *data_k = data+k;
    for(int i=0;i<len;i+=32) {
      rvv_ibtf_xor32_hi(data_k+i, data_k+len+i);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_ibtf_xor32_lo_1src(data_k+i, temp2);
    }
    k += blk_size;
    count++;
  }
  if(k<datalen) {
    // count = 1;
    uint32_t *data_k = data+k;
    for(int i=0;i<len;i+=32) {
      rvv_ibtf_xor32_hi(data_k+i, data_k+len+i);
      gf232v_mul_0x2(temp,&data_k[len+i]);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_ibtf_xor32_lo_2src(data_k+i, temp, temp2);
    }
    k += blk_size;
    count++;
  }
  if(k<datalen) {
    // count = 2;
    uint32_t *data_k = data+k;
    for(int i=0;i<len;i+=32) {
      rvv_ibtf_xor32_hi(data_k+i, data_k+len+i);
      gf232v_mul_0x5(temp,&data_k[len+i]);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_ibtf_xor32_lo_2src(data_k+i, temp, temp2);
    }
    k += blk_size;
    count++;
  }

  for(;k<datalen;k+=blk_size) {
    uint32_t si_a = s0_gft[count++];

    uint32_t *data_k = data+k;
    if( 16 > si_a )
    for(int i=0;i<len;i+=32) {
      rvv_ibtf_xor32_hi(data_k+i, data_k+len+i);
      gf232v_mulscalar_gf16(temp,&data_k[len+i],si_a);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_ibtf_xor32_lo_2src(data_k+i, temp, temp2);
    }
    else // if (256 > si_a )
    for(int i=0;i<len;i+=32) {
      rvv_ibtf_xor32_hi(data_k+i, data_k+len+i);
      gf232v_mulscalar_gf256(temp,&data_k[len+i],si_a);
      (*gft_mul_vi[si])( temp2 , &data_k[len+i] );
      rvv_ibtf_xor32_lo_2src(data_k+i, temp, temp2);
    }
  }
 
}



///////////////////////////////////////////////////////



static
void ibtfy_layer_s4( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s4_gf16_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf16( tmp , data+i+32 , ptr+(pi<<2) );
    (*gft_mul_vi[4])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0000ffff, 16);
    i+=64;
    if( (++pi) >= S4_GF16_NUMX32 ) break;
  }

  ptr = s4_gf256_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[4])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0000ffff, 16);
    i+=64;
    ++pi;
  }
}



static
void ibtfy_layer_s3( uint32_t*data, int datalen )
{
	uint32_t tmp[32];
	uint32_t tmp2[32];

	const uint32_t *ptr;
	int pi;

	int i = 0;
	ptr = s3_gf256_vec;
	pi = 0;
	while (i < datalen) {
		rvv_ibtf_xor32_hi(data+i, data+i+32);
		gf232v_mul_gf256(tmp, data + i + 32, ptr + (pi << 3));
		(*gft_mul_vi[3])(tmp2, data + i + 32);
		rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
		// reorder data (Iter 3: RVV SWAPMOVE)
		RVV_SWAPMOVE32(data+i, data+i+32, 0x00ff00ff, 8);
		i += 64;
		if ((++pi) >= S3_GF256_NUMX32) break;
	}

	ptr = s3_gf216_vec;
	pi = 0;
	while (i < datalen) {
		rvv_ibtf_xor32_hi(data+i, data+i+32);
		gf232v_mul_gf216(tmp, data + i + 32, ptr + (pi << 4));
		(*gft_mul_vi[3])(tmp2, data + i + 32);
		rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
		// reorder data (Iter 3: RVV SWAPMOVE)
		RVV_SWAPMOVE32(data+i, data+i+32, 0x00ff00ff, 8);
		i += 64;
		++pi;
	}
}




static
void ibtfy_layer_s2( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s2_gf256_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[2])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0f0f0f0f, 4);
    i+=64;
    if( (++pi) >= S2_GF256_NUMX32 ) break;
  }

  ptr = s2_gf216_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf216( tmp , data+i+32 , ptr+(pi<<4) );
    (*gft_mul_vi[2])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x0f0f0f0f, 4);
    i+=64;
    ++pi;
  }
}




static
void ibtfy_layer_s1( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s1_gf256_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[1])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x33333333, 2);
    i+=64;
    if( (++pi) >= S1_GF256_NUMX32 ) break;
  }

  ptr = s1_gf216_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf216( tmp , data+i+32 , ptr+(pi<<4) );
    (*gft_mul_vi[1])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x33333333, 2);
    i+=64;
    ++pi;
  }
}




static
void ibtfy_layer_s0( uint32_t*data, int datalen )
{
  uint32_t tmp[32];
  uint32_t tmp2[32];

  const uint32_t *ptr;
  int pi;

  int i = 0;
  ptr = s0_gf256_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf256( tmp , data+i+32 , ptr+(pi<<3) );
    (*gft_mul_vi[0])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x55555555, 1);
    i+=64;
    if( (++pi) >= S0_GF256_NUMX32 ) break;
  }

  ptr = s0_gf216_vec;
  pi = 0;
  while(i<datalen) {
    rvv_ibtf_xor32_hi(data+i, data+i+32);
    gf232v_mul_gf216( tmp , data+i+32 , ptr+(pi<<4) );
    (*gft_mul_vi[0])( tmp2 , data+i+32 );
    rvv_ibtf_xor32_lo_2src(data+i, tmp, tmp2);
    // reorder data (Iter 3: RVV SWAPMOVE)
    RVV_SWAPMOVE32(data+i, data+i+32, 0x55555555, 1);
    i+=64;
    ++pi;
  }
}




///////////////////////////////////////////////////



void ibtfy_32768( uint32_t * poly )
{
  int datalen = 1024;

  gft_mul_vi[0] = gft_mul_v26;
  gft_mul_vi[1] = gft_mul_v25;
  gft_mul_vi[2] = gft_mul_v24;
  gft_mul_vi[3] = gft_mul_v23;
  gft_mul_vi[4] = gft_mul_v22;
  gft_mul_vi[5] = gft_mul_v21;
  gft_mul_vi[6] = gft_mul_v20;
  gft_mul_vi[7] = gft_mul_v19;
  gft_mul_vi[8] = gft_mul_v18;
  gft_mul_vi[9] = gft_mul_v17;

  ibtfy_layer_s0( poly , datalen );
  ibtfy_layer_s1( poly , datalen );
  ibtfy_layer_s2( poly , datalen );
  ibtfy_layer_s3( poly , datalen );
  ibtfy_layer_s4( poly , datalen );

  ibtfy_layer( poly , datalen , 5 );
  ibtfy_layer( poly , datalen , 6 );
  ibtfy_layer( poly , datalen , 7 );
  ibtfy_layer( poly , datalen , 8 );
  ibtfy_layer( poly , datalen , 9 );
}


void ibtfy_65536( uint32_t * poly )
{
  int datalen = 2048;

  gft_mul_vi[0] = gft_mul_v27;
  gft_mul_vi[1] = gft_mul_v26;
  gft_mul_vi[2] = gft_mul_v25;
  gft_mul_vi[3] = gft_mul_v24;
  gft_mul_vi[4] = gft_mul_v23;
  gft_mul_vi[5] = gft_mul_v22;
  gft_mul_vi[6] = gft_mul_v21;
  gft_mul_vi[7] = gft_mul_v20;
  gft_mul_vi[8] = gft_mul_v19;
  gft_mul_vi[9] = gft_mul_v18;
  gft_mul_vi[10] = gft_mul_v17;

  ibtfy_layer_s0( poly , datalen );
  ibtfy_layer_s1( poly , datalen );
  ibtfy_layer_s2( poly , datalen );
  ibtfy_layer_s3( poly , datalen );
  ibtfy_layer_s4( poly , datalen );

  ibtfy_layer( poly , datalen , 5 );
  ibtfy_layer( poly , datalen , 6 );
  ibtfy_layer( poly , datalen , 7 );
  ibtfy_layer( poly , datalen , 8 );
  ibtfy_layer( poly , datalen , 9 );
  ibtfy_layer( poly , datalen , 10 );
}

//new code block
void ibtfy_131072(uint32_t * poly)
{
	int datalen = 4096;

	gft_mul_vi[0] = gft_mul_v28;
	gft_mul_vi[1] = gft_mul_v27;
	gft_mul_vi[2] = gft_mul_v26;
	gft_mul_vi[3] = gft_mul_v25;
	gft_mul_vi[4] = gft_mul_v24;
	gft_mul_vi[5] = gft_mul_v23;
	gft_mul_vi[6] = gft_mul_v22;
	gft_mul_vi[7] = gft_mul_v21;
	gft_mul_vi[8] = gft_mul_v20;
	gft_mul_vi[9] = gft_mul_v19;
	gft_mul_vi[10] = gft_mul_v18;
	gft_mul_vi[11] = gft_mul_v17;

	ibtfy_layer_s0(poly, datalen);
	ibtfy_layer_s1(poly, datalen);
	ibtfy_layer_s2(poly, datalen);
	ibtfy_layer_s3(poly, datalen);
	ibtfy_layer_s4(poly, datalen);

	ibtfy_layer(poly, datalen, 5);
	ibtfy_layer(poly, datalen, 6);
	ibtfy_layer(poly, datalen, 7);
	ibtfy_layer(poly, datalen, 8);
	ibtfy_layer(poly, datalen, 9);
	ibtfy_layer(poly, datalen, 10);
	ibtfy_layer(poly, datalen, 11);
}

