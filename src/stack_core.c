#include "push_swap.h"

void	st_swap(t_stack *s)
{
	t_node	*a;
	t_node	*b;

	if (!s || !s->top || !s->top->next)
		return ;
	a = s->top;
	b = s->top->next;
	a->next = b->next;
	b->next = a;
	s->top = b;
}

void	st_push(t_stack *src, t_stack *dst)
{
	t_node	*tmp;

	if (!src || !src->top)
		return ;
	tmp = src->top;
	src->top = tmp->next;
	tmp->next = dst->top;
	dst->top = tmp;
	src->size--;
	dst->size++;
}

void	st_rotate(t_stack *s)
{
	t_node	*first;
	t_node	*last;

	if (!s || !s->top || !s->top->next)
		return ;
	first = s->top;
	last = s->top;
	while (last->next)
		last = last->next;
	s->top = first->next;
	first->next = NULL;
	last->next = first;
}

void	st_rrotate(t_stack *s)
{
	t_node	*prev;
	t_node	*curr;

	if (!s || !s->top || !s->top->next)
		return ;
	prev = NULL;
	curr = s->top;
	while (curr->next)
	{
		prev = curr;
		curr = curr->next;
	}
	if (prev)
		prev->next = NULL;
	curr->next = s->top;
	s->top = curr;
}
