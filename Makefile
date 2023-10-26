
cshell: cshell.c
	gcc -Wall -Wextra -Werror -o cshell cshell.c

clean:
	rm cshell