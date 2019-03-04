all:
	gcc -o main main.c

check:
	cppcheck main.c

sanitized:
	gcc -o main main.c -fsanitize=address
	

