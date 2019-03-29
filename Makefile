# To compile with test1, make test1
# To compile with test2, make test2
CC = clang -g -Wall
EXECUTABLE=sfs

SOURCES_TEST1= tests/disk_emu.c sfs_api.c tests/sfs_test1.c tests/tests.c
SOURCES_TEST2= tests/disk_emu.c sfs_api.c tests/sfs_test2.c tests/tests.c

test1: $(SOURCES_TEST1) 
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST1)

test2: $(SOURCES_TEST2)
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST2)
clean:
	rm $(EXECUTABLE)
