
TARGET = mls

$(TARGET): mls.c mls.h colors.c tags.c help.c
	clang -Wall -O3 $< -o $@

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

uninstall:
	rm -f $(INSTALL_PATH)/bin/$(TARGET)
