NAME =	libosumapparser.a

FILE =	osu_map_parser.c	\

SRC =	$(FILE:%.c=src/%.o)

OBJ =	$(SRC:.c=.o)

INC =	-Iinclude

CFLAGS=	$(INC)	\
	-W	\
	-Wall	\
	-Wextra	\

CC =	gcc

all:	$(NAME)

$(NAME):$(OBJ)
	$(AR) rc $(NAME) $(OBJ)

clean:
	$(RM) $(OBJ)

fclean:	clean
	$(RM) $(NAME)

re:	fclean all

dbg:	CFLAGS += -g -O0
dbg:	re
