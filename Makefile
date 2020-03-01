
TARGET = mls

$(TARGET): mls.c colors.c help.c
	gcc -O3 $< -o $@

colors.c: colors.gperf
	gperf -N 'get_color_entry' -H 'hash_extension' -e ' ' $< > $@

help.c: mls.1
	man ./mls.1 | cat | head -n-1 | tail -n+7 > help
	echo -e '\0' >> help
	xxd -i help > $@
	rm -f help

clean:
	rm -f $(TARGET) colors.c help.c help

INSTALL_PATH = /usr/local

install: $(TARGET)
	mkdir -p $(INSTALL_PATH)/bin
	cp -f $(TARGET) $(INSTALL_PATH)/bin
	chmod 755 $(INSTALL_PATH)/bin/$(TARGET)

uninstall:
	rm -f $(INSTALL_PATH)/bin/$(TARGET)
