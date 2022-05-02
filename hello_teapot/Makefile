OBJS=triangle.o video.o models.o
BIN=hello_teapot.bin
LDFLAGS+= -ldrm -lgbm -lEGL -lGL -lm -lpthread -L/opt/vc/lib/ -lbcm_host -lvcos -lvchiq_arm -L/opt/vc/src/hello_pi/libs/ilclient -lilclient -lvcilcs
INCLUDES+= -I/usr/include/libdrm -I/opt/vc/include -I/opt/vc/src/hello_pi/libs/ilclient

all: $(BIN) $(LIB)

%.o: %.c
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

%.o: %.cpp
	@rm -f $@ 
	$(CXX) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

%.bin: $(OBJS)
	$(CC) -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

%.a: $(OBJS)
	$(AR) r $@ $^
