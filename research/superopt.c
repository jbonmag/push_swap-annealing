/*
** Window superoptimizer: reads an op list on stdin and writes an equivalent
** shorter one. Every op word up to length D is enumerated once on long symbolic
** stacks, and the shortest word per resulting state goes in a hash table. A
** window of the input is replaced when the table holds a shorter word with the
** same effect. The stacks must be deeper than the window for the effect to be
** the same, so windows near small stacks are skipped.
**
** usage: superopt N [D] < ops > ops   (N elements initially in a)
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYM 16
#define WMAX 12
#define MAXOPS 200000
#define HBITS 24

typedef struct s_sym
{
	int8_t	a[2 * SYM + WMAX];
	int8_t	b[2 * SYM + WMAX];
	int		na;
	int		nb;
}	t_sym;

typedef struct s_ent
{
	uint64_t	key;
	uint8_t		len;
	uint8_t		w[WMAX];
}	t_ent;

static const char	*g_names[] = {"sa", "sb", "ss", "pa", "pb", "ra", "rb", "rr",
	"rra", "rrb", "rrr"};
static t_ent		*g_tab;
static long			g_cnt;

static void	s_swap(int8_t *x, int m)
{
	int8_t	t;

	if (m >= 2)
	{
		t = x[0];
		x[0] = x[1];
		x[1] = t;
	}
}

static void	s_rot(int8_t *x, int m)
{
	int8_t	t;

	if (m >= 2)
	{
		t = x[0];
		memmove(x, x + 1, m - 1);
		x[m - 1] = t;
	}
}

static void	s_rrot(int8_t *x, int m)
{
	int8_t	t;

	if (m >= 2)
	{
		t = x[m - 1];
		memmove(x + 1, x, m - 1);
		x[0] = t;
	}
}

static void	apply(t_sym *s, int op)
{
	if (op == 0 || op == 2)
		s_swap(s->a, s->na);
	if (op == 1 || op == 2)
		s_swap(s->b, s->nb);
	if (op == 3)
	{
		memmove(s->a + 1, s->a, s->na);
		s->a[0] = s->b[0];
		memmove(s->b, s->b + 1, s->nb - 1);
		s->na++;
		s->nb--;
	}
	if (op == 4)
	{
		memmove(s->b + 1, s->b, s->nb);
		s->b[0] = s->a[0];
		memmove(s->a, s->a + 1, s->na - 1);
		s->nb++;
		s->na--;
	}
	if (op == 5 || op == 7)
		s_rot(s->a, s->na);
	if (op == 6 || op == 7)
		s_rot(s->b, s->nb);
	if (op == 8 || op == 10)
		s_rrot(s->a, s->na);
	if (op == 9 || op == 10)
		s_rrot(s->b, s->nb);
}

static void	s_init(t_sym *s)
{
	int	i;

	s->na = SYM;
	s->nb = SYM;
	for (i = 0; i < SYM; i++)
	{
		s->a[i] = i;
		s->b[i] = SYM + i;
	}
}

static uint64_t	s_key(const t_sym *s)
{
	uint64_t	h = 0xcbf29ce484222325ULL ^ (uint64_t)s->na;
	int			i;

	for (i = 0; i < s->na; i++)
		h = (h ^ (uint8_t)s->a[i]) * 0x100000001b3ULL;
	h = (h ^ 0xFF) * 0x100000001b3ULL;
	for (i = 0; i < s->nb; i++)
		h = (h ^ (uint8_t)s->b[i]) * 0x100000001b3ULL;
	return (h | 1);
}

static t_ent	*tab_slot(uint64_t k)
{
	uint64_t	i = k & ((1ULL << HBITS) - 1);

	while (g_tab[i].key && g_tab[i].key != k)
		i = (i + 1) & ((1ULL << HBITS) - 1);
	return (&g_tab[i]);
}

/* breadth first, so the first word that reaches a state is a shortest one */
static void	build_table(int depth)
{
	static t_ent	*cur, *nxt;
	long			nc = 1, nn, i;
	int				d, op, j;
	t_sym			s;

	cur = malloc(sizeof(t_ent) * (1L << HBITS));
	nxt = malloc(sizeof(t_ent) * (1L << HBITS));
	memset(&cur[0], 0, sizeof(t_ent));
	s_init(&s);
	*tab_slot(s_key(&s)) = (t_ent){s_key(&s), 0, {0}};
	for (d = 1; d <= depth; d++)
	{
		nn = 0;
		for (i = 0; i < nc; i++)
			for (op = 0; op < 11; op++)
			{
				t_ent	e = cur[i], *slot;

				s_init(&s);
				for (j = 0; j < e.len; j++)
					apply(&s, e.w[j]);
				apply(&s, op);
				e.key = s_key(&s);
				slot = tab_slot(e.key);
				if (slot->key)
					continue ;
				e.w[e.len++] = op;
				*slot = e;
				nxt[nn++] = e;
				g_cnt++;
			}
		{
			t_ent	*t = cur;

			cur = nxt;
			nxt = t;
		}
		nc = nn;
	}
	free(cur);
	free(nxt);
}

static int	op_id(const char *w)
{
	int	i;

	for (i = 0; i < 11; i++)
		if (!strcmp(w, g_names[i]))
			return (i);
	exit(2);
}

/* one left-to-right sweep; returns the ops removed */
static int	sweep(int *ops, int *len, int n, int wmax)
{
	int	i = 0, w = 0, removed = 0, na = n, nb = 0, l, j, best;
	t_ent	*hit;
	static int	out[MAXOPS];

	while (i < *len)
	{
		best = 0;
		hit = NULL;
		if (na > WMAX + 1 && nb > WMAX + 1)
		{
			t_sym	s;

			s_init(&s);
			for (l = 1; l <= wmax && i + l <= *len; l++)
			{
				t_ent	*e;

				apply(&s, ops[i + l - 1]);
				e = tab_slot(s_key(&s));
				if (e->key && e->len < l && l - e->len > best)
				{
					best = l - e->len;
					hit = e;
					j = l;
				}
			}
		}
		if (hit)
		{
			for (l = 0; l < hit->len; l++)
			{
				out[w++] = hit->w[l];
				if (hit->w[l] == 3) { na++; nb--; }
				if (hit->w[l] == 4) { na--; nb++; }
			}
			removed += best;
			i += j;
			continue ;
		}
		if (ops[i] == 3) { na++; nb--; }
		if (ops[i] == 4) { na--; nb++; }
		out[w++] = ops[i++];
	}
	memcpy(ops, out, sizeof(int) * w);
	*len = w;
	return (removed);
}

int	main(int ac, char **av)
{
	static int	ops[MAXOPS];
	char		w[16];
	int			n = atoi(av[1]), depth = ac > 2 ? atoi(av[2]) : 7, len = 0, i;
	int			orig, r;

	g_tab = calloc(1L << HBITS, sizeof(t_ent));
	build_table(depth);
	while (scanf("%15s", w) == 1)
		ops[len++] = op_id(w);
	orig = len;
	do
		r = sweep(ops, &len, n, WMAX);
	while (r > 0);
	fprintf(stderr, "table=%ld %d -> %d\n", g_cnt, orig, len);
	for (i = 0; i < len; i++)
		puts(g_names[ops[i]]);
	return (0);
}
