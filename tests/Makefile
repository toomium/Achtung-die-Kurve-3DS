# Run: make -C tests (needs the SDL 1.2 headers only). config_test is built twice,
# the second time for the 3DS side of the settings file.
test:
	$(CC) -Wall -Wextra -I../src -o config_test config_test.c
	./config_test
	$(CC) -Wall -Wextra -D__3DS__ -DCONFIG_DIR_3DS='"/tmp/zatackax-3ds-test"' -I../src -I../3ds/source -o config_test config_test.c
	./config_test
	$(CC) -Wall -Wextra -I../src -o ai_test ai_test.c -lm
	./ai_test
	$(CC) -Wall -Wextra -D__3DS__ -I../src -o stats_test stats_test.c
	./stats_test
	$(CC) -Wall -Wextra -D__3DS__ -I../src -o items_test items_test.c -lSDL
	./items_test
	$(CC) -Wall -Wextra -D__3DS__ -I../src -o tournament_test tournament_test.c -lSDL
	./tournament_test
	rm -f config_test ai_test stats_test items_test tournament_test
