# $(CC) is a varible in make that is cc by default
# -Wall stands for All Warnings
# -Wextra and -pendantic for even more warning 
# Good for development but very bad for cp
# C17 modern C is very good for development of low level stuff 

sota: sota.c
	$(CC) sota.c -o sota -Wall -Wextra -pedantic -std=c17