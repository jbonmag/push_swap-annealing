#include "sa.h"

static int	count_range(const t_u64 *b, int lo, int hi)
{
	int	wl;
	int	wh;
	int	c;

	if (lo >= hi)
		return (0);
	wl = lo >> 6;
	wh = hi >> 6;
	if (wl == wh)
		return (sa_popcount(b[wl]
				& ((1ULL << (hi & 63)) - (1ULL << (lo & 63)))));
	c = sa_popcount(b[wl] >> (lo & 63));
	wl++;
	while (wl < wh)
		c += sa_popcount(b[wl++]);
	if (hi & 63)
		c += sa_popcount(b[wh] & ((1ULL << (hi & 63)) - 1));
	return (c);
}

/*
** Set bits met walking cyclically from `from` up to `to`, excluding `to`.
*/
int	sa_cyc(const t_u64 *b, int from, int to, int dom)
{
	if (from <= to)
		return (count_range(b, from, to));
	return (count_range(b, from, dom) + count_range(b, 0, to));
}

int	sa_next_set(const t_u64 *b, int c, int dom)
{
	int		w;
	int		i;
	t_u64	v;

	w = c >> 6;
	v = b[w] & (~0ULL << (c & 63));
	if (v && (w << 6) + sa_ctz(v) < dom)
		return ((w << 6) + sa_ctz(v));
	i = w + 1;
	while (i <= (dom - 1) >> 6)
	{
		if (b[i])
			return ((i << 6) + sa_ctz(b[i]));
		i++;
	}
	i = 0;
	while (i <= w)
	{
		if (b[i])
			return ((i << 6) + sa_ctz(b[i]));
		i++;
	}
	return (c);
}

void	sa_bit_set(t_u64 *b, int i)
{
	b[i >> 6] |= 1ULL << (i & 63);
}

void	sa_bit_clear(t_u64 *b, int i)
{
	b[i >> 6] &= ~(1ULL << (i & 63));
}
