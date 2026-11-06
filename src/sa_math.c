#include "sa.h"

static double	ln_series(double y)
{
	double	term;
	double	sum;
	int		i;

	term = y;
	sum = 0.0;
	i = 1;
	while (i < 40)
	{
		sum += term / i;
		term *= y * y;
		i += 2;
	}
	return (2.0 * sum);
}

double	sa_ln(double v)
{
	int	k;

	k = 0;
	while (v >= 2.0)
	{
		v /= 2.0;
		k++;
	}
	while (v < 1.0)
	{
		v *= 2.0;
		k--;
	}
	return (ln_series((v - 1.0) / (v + 1.0)) + k * 0.6931471805599453);
}

double	sa_exp(double v)
{
	double	term;
	double	sum;
	int		halves;
	int		i;

	halves = 0;
	while (v > 0.5 || v < -0.5)
	{
		v /= 2.0;
		halves++;
	}
	term = 1.0;
	sum = 1.0;
	i = 1;
	while (i < 16)
	{
		term *= v / i;
		sum += term;
		i++;
	}
	while (halves-- > 0)
		sum *= sum;
	return (sum);
}

/*
** Geometric cooling: the temperature after a fraction of the way from t0 to t1.
*/
double	sa_temp_at(double t0, double t1, double frac)
{
	return (sa_exp(sa_ln(t0) * (1.0 - frac) + sa_ln(t1) * frac));
}
