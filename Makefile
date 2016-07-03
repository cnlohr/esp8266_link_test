TARGET_OUT:=server.elf client.elf
SERVER_FW_FILES:=server.elf-0x00000.bin server.elf-0x40000.bin
CLIENT_FW_FILES:=client.elf-0x00000.bin client.elf-0x40000.bin
all : $(TARGET_OUT) $(SERVER_FW_FILES) $(CLIENT_FW_FILES)


SRCS:=driver/uart.c \
	common/mystuff.c \
	user/ws2812_i2s.c \
	user/user_main.c 

GCC_FOLDER:=~/esp8266/esp-open-sdk/xtensa-lx106-elf
ESPTOOL_PY:=~/esp8266/esptool/esptool.py
EXPTOOL_CK:=~/esp8266/esptool-ck/esptool
SDK:=/home/cnlohr/esp8266/ESP8266_NONOS_SDK
PORT:=/dev/ttyUSB0
#PORT:=/dev/ttyACM0

XTLIB:=$(SDK)/lib
XTGCCLIB:=$(GCC_FOLDER)/lib/gcc/xtensa-lx106-elf/4.8.2/libgcc.a
FOLDERPREFIX:=$(GCC_FOLDER)/bin
PREFIX:=$(FOLDERPREFIX)/xtensa-lx106-elf-
CC:=$(PREFIX)gcc

CFLAGS:=-mlongcalls -I$(SDK)/include -Imyclib -Iinclude -Iuser -Os -I$(SDK)/include/ -Icommon -DICACHE_FLASH

#	   \
#

LDFLAGS_CORE:=\
	-nostdlib \
	-Wl,--relax -Wl,--gc-sections \
	-L$(XTLIB) \
	-L$(XTGCCLIB) \
	$(SDK)/lib/liblwip.a \
	$(SDK)/lib/libssl.a \
	$(SDK)/lib/libupgrade.a \
	$(SDK)/lib/libnet80211.a \
	$(SDK)/lib/liblwip.a \
	$(SDK)/lib/libwpa.a \
	$(SDK)/lib/libnet80211.a \
	$(SDK)/lib/libphy.a \
	$(SDK)/lib/libcrypto.a \
	$(SDK)/lib/libmain.a \
	$(SDK)/lib/libpp.a \
	$(XTGCCLIB) \
	-T $(SDK)/ld/eagle.app.v6.ld

LINKFLAGS:= \
	$(LDFLAGS_CORE) \
	-B$(XTLIB)

#image.elf : $(OBJS)
#	$(PREFIX)ld $^ $(LDFLAGS) -o $@

$(TARGET_OUT) : $(SRCS)
	$(PREFIX)gcc $(CFLAGS) $^  -flto $(LINKFLAGS) -o client.elf
	$(PREFIX)gcc $(CFLAGS) $^  -flto $(LINKFLAGS) -DSERVER -o server.elf
	#nm -S -n $(TARGET_OUT) > image.map
	#$(PREFIX)objdump -S $@ > image.lst



$(SERVER_FW_FILES): server.elf
	@echo "FW $@"
	PATH=$(FOLDERPREFIX):$$PATH;$(ESPTOOL_PY) elf2image server.elf

$(CLIENT_FW_FILES): client.elf
	@echo "FW $@"
	PATH=$(FOLDERPREFIX):$$PATH;$(ESPTOOL_PY) elf2image client.elf

burn_server : $(SERVER_FW_FILES)
	stty -F $(PORT) 115200 -echo raw
	sleep .1
	($(ESPTOOL_PY) --port $(PORT) write_flash -fs 8m -fm dout 0x00000 server.elf-0x00000.bin 0x40000 server.elf-0x40000.bin)||(true)


burn_client : $(CLIENT_FW_FILES)
	stty -F $(PORT) 115200 -echo raw
	sleep .1
	($(ESPTOOL_PY) --port $(PORT) write_flash -fs 8m -fm dout 0x00000 client.elf-0x00000.bin 0x40000 client.elf-0x40000.bin)||(true)

clean :
	rm -rf user/*.o driver/*.o $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2) $(SERVER_FW_FILES) $(CLIENT_FW_FILES)


