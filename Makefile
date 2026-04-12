# this Makefile was human-written

CC	   	 		=	gcc
OPTS   	 		=	-std=c11 -Wall -Wextra -pedantic -O0
IN	   	 		=	clorn.c
OUT				=	clorn
TEST_IN			=	test.rn
TEST_OUT		=	out.c
TEST_OUT_OUT	=	test

all:
	$(CC) $(OPTS) $(IN) -o $(OUT)

test: all
	./$(OUT) $(TEST_IN) $(TEST_OUT)
	$(CC) $(OPTS) $(TEST_OUT) -o $(TEST_OUT_OUT)
	./$(TEST_OUT_OUT)

clean:
	rm -v $(OUT)

cleantest: clean
	rm -v $(TEST_OUT) $(TEST_OUT_OUT)
