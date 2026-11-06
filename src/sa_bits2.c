#include "sa.h"

int	sa_bit_get(const t_u64 *b, int i)
{
	return ((b[i >> 6] >> (i & 63)) & 1);
}

/*
** Set bits of v, by adding neighbouring fields of growing width.
*/
int	sa_popcount(t_u64 v)
{
	v = v - ((v >> 1) & 0x5555555555555555ULL);
	v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
	v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
	return ((int)((v * 0x0101010101010101ULL) >> 56));
}

/*
** Index of the lowest set bit of a non-zero v: the bits below it, counted.
*/
int	sa_ctz(t_u64 v)
{
	return (sa_popcount((v & (~v + 1)) - 1));
}
