NAME    = push_swap
CC      = cc
CFLAGS  = -Wall -Wextra -Werror -O2 -flto
SRCDIR  = src

SRC     = push_swap.c parse.c stack_core.c stack_ops.c utils.c normalize.c \
          small_sort.c solve.c solution.c radix_sort.c swapify.c \
          sa_bits.c sa_bits2.c sa_math.c sa_rng.c sa_step.c sa_run.c sa_eval.c \
          sa_span.c sa_emit.c sa_keep.c sa_seed.c sa_greedy.c sa_pick.c \
          sa_order.c sa_local_prep.c sa_local.c sa_pi.c sa_pi_delta.c sa_pair.c sa_toggle.c \
          sa_anneal.c sa_arms.c sa_race.c sa_solve.c
OBJ     = $(SRC:.c=.o)
SRCS    = $(addprefix $(SRCDIR)/, $(SRC))
OBJS    = $(addprefix $(SRCDIR)/, $(OBJ))

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(NAME) $(OBJS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/push_swap.h $(SRCDIR)/sa.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
