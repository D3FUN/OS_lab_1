CC = cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2

ifeq ($(shell uname -s),Darwin)
LIB_EXT = dylib
SHARED = -dynamiclib -Wl,-install_name,@rpath/$(@F)
RPATH = -Wl,-rpath,@loader_path
else
LIB_EXT = so
SHARED = -shared -Wl,-soname,$(@F)
RPATH = -Wl,-rpath,'$$ORIGIN'
DL_LIBS = -ldl
endif

.PHONY: all clean
all: linked dynamic libtrial.$(LIB_EXT) libsieve.$(LIB_EXT)

libtrial.$(LIB_EXT): trial.c contracts.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -fPIC $(SHARED) -o $@ trial.c -lm

libsieve.$(LIB_EXT): sieve.c contracts.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -fPIC $(SHARED) -o $@ sieve.c -lm

linked: linked.c commands.c commands.h contracts.h libtrial.$(LIB_EXT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ linked.c commands.c -L. -ltrial $(RPATH)

dynamic: dynamic.c commands.c commands.h libtrial.$(LIB_EXT) libsieve.$(LIB_EXT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -DLIB_EXT='".$(LIB_EXT)"' -o $@ dynamic.c commands.c $(DL_LIBS)

clean:
	rm -f linked dynamic libtrial.so libsieve.so libtrial.dylib libsieve.dylib
