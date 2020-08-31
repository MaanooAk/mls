
TARGET = mls

$(TARGET): mls.c mls.h colors.c tags.c help.c
	cc -Wall -O3 $< -o $@

%.c: %.gperf
	gperf -e ' ' $< > $@

%.h: %.c
	cat $< | grep '^\w.* {$$' | sed 's/ {/;/' > $@

help.c: mls.1
	man ./mls.1 | head -n-1 | tail -n+7 | sed -e 's/^/"/' -e 's/$$/\\n"/' | (echo 'static const char* help = '; cat; echo ';') > $@

clean:
	rm -f $(TARGET) mls.h colors.c tags.c help.c

INSTALL_PATH = /usr/local

install: $(TARGET)
	mkdir -p $(INSTALL_PATH)/bin
	cp -f $(TARGET) $(INSTALL_PATH)/bin
	chmod 755 $(INSTALL_PATH)/bin/$(TARGET)
	ln -fs $(TARGET) $(INSTALL_PATH)/bin/t$(TARGET)
	mkdir -p $(INSTALL_PATH)/share/man/man1/
	cp -f $(TARGET).1 $(INSTALL_PATH)/share/man/man1/$(TARGET).1
	chmod 644 $(INSTALL_PATH)/share/man/man1/$(TARGET).1

uninstall:
	rm -f $(INSTALL_PATH)/bin/$(TARGET)
	rm -f $(INSTALL_PATH)/bin/t$(TARGET)
	rm -f $(INSTALL_PATH)/share/man/man1/$(TARGET).1
