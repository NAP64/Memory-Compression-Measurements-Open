MKFILE_PATH=$(abspath $(lastword $(MAKEFILE_LIST)))
WORK_PATH=$(patsubst %/Makefile,%,$(MKFILE_PATH))
INCLUDE=$(WORK_PATH)/include
IFLAGS=-I$(INCLUDE)

LDLIBS=-pthread -ldl
CFLAGS=-ggdb3  -Wall
DFLAGS=
SFLAGS=-shared -fPIC

SRCDIR=$(WORK_PATH)/src
TARGET=$(WORK_PATH)/bin
SRC_C=$(wildcard $(SRCDIR)/*.c)
BIN_DRIVER=$(patsubst $(SRCDIR)/%.c,$(TARGET)/%,$(SRC_C))

COMPRESSIONDIR=$(SRCDIR)/compression
COMPRESSIONTARGET=$(TARGET)/compression
COMPRESSION_C=$(wildcard $(COMPRESSIONDIR)/*.c)
COMPRESSION_SO=$(patsubst $(COMPRESSIONDIR)/%.c,$(COMPRESSIONTARGET)/%.so,$(COMPRESSION_C))
COMPRESSION_SO_SHORT=$(patsubst $(COMPRESSIONTARGET)/%.so,%,$(COMPRESSION_SO))
COMPRESSION_SDIR=$(wildcard $(COMPRESSIONDIR)/*/Makefile)
COMPRESSION_SDIR_SO=$(patsubst $(COMPRESSIONDIR)/%/Makefile, $(COMPRESSIONTARGET)/%.so, $(COMPRESSION_SDIR))
COMPRESSION_SDIR_SHORT=$(patsubst $(COMPRESSIONTARGET)/%.so,%,$(COMPRESSION_SDIR_SO))

LAYOUTDIR=$(SRCDIR)/layout
LAYOUTTARGET=$(TARGET)/layout
LAYOUT_C=$(wildcard $(LAYOUTDIR)/*.c)
LAYOUT_SO=$(patsubst $(LAYOUTDIR)/%.c,$(LAYOUTTARGET)/%.so,$(LAYOUT_C))
LAYOUT_SO_SHORT=$(patsubst $(LAYOUTTARGET)/%.so,%,$(LAYOUT_SO))
LAYOUT_SDIR=$(wildcard $(LAYOUTDIR)/*/Makefile)
LAYOUT_SDIR_SO=$(patsubst $(LAYOUTDIR)/%/Makefile, $(LAYOUTTARGET)/%.so, $(LAYOUT_SDIR))
LAYOUT_SDIR_SHORT=$(patsubst $(LAYOUTTARGET)/%.so,%,$(LAYOUT_SDIR_SO))

default: bootstrap driver $(COMPRESSION_SO) $(LAYOUT_SO) $(COMPRESSION_SDIR_SO) $(LAYOUT_SDIR_SO)
driver: $(BIN_DRIVER)

#For individual compilations
$(COMPRESSION_SO_SHORT):
	@$(MAKE) $(COMPRESSIONTARGET)/$@.so DFLAGS="$(DFLAGS)"

$(COMPRESSION_SDIR_SHORT):
	@$(MAKE) $(COMPRESSIONTARGET)/$@.so DFLAGS="$(DFLAGS)"

$(LAYOUT_SO_SHORT):
	@$(MAKE) $(LAYOUTTARGET)/$@.so DFLAGS="$(DFLAGS)"

$(LAYOUT_SDIR_SHORT):
	@$(MAKE) $(LAYOUTTARGET)/$@.so DFLAGS="$(DFLAGS)"

# The rules assume each compression is in its own file
$(COMPRESSION_SO): $(COMPRESSIONTARGET)/%.so : $(COMPRESSIONDIR)/%.c
	$(CC) $(DFLAGS) $(CFLAGS) $(SFLAGS) $(IFLAGS) -o $@ $< -lrt -lm

# The rules assume each layout is in its own file
$(LAYOUT_SO): $(LAYOUTTARGET)/%.so : $(LAYOUTDIR)/%.c
	$(CC) $(DFLAGS) $(CFLAGS) $(SFLAGS) $(IFLAGS) -o $@ $<

# The rules assume each compression that needs its own make is in its own folder
$(COMPRESSION_SDIR_SO): $(COMPRESSIONTARGET)/%.so: $(COMPRESSIONDIR)/%/
	$(MAKE) -C $< DFLAGS="$(DFLAGS)" IFLAGS=$(IFLAGS) TARGET=$(COMPRESSIONTARGET) INCLUDE=$(INCLUDE)

# The rules assume each layout that needs its own make is in its own folder
$(LAYOUT_SDIR_SO): $(LAYOUTTARGET)/%.so: $(LAYOUTDIR)/%/
	$(MAKE) -C $< DFLAGS="$(DFLAGS)" IFLAGS=$(IFLAGS) TARGET=$(LAYOUTTARGET) INCLUDE=$(INCLUDE) COMPDIR=$(COMPRESSIONDIR)


$(BIN_DRIVER): $(TARGET)/%: $(SRCDIR)/%.c
	$(CC) $(DFLAGS) $(CFLAGS) $(IFLAGS) -o $@ $< $(LDLIBS) 

list:
	@echo "compression:" $(COMPRESSION_SO_SHORT) $(COMPRESSION_SDIR_SHORT)
	@echo "layout:" $(LAYOUT_SO_SHORT) $(LAYOUT_SDIR_SHORT)
	@echo $(COMPRESSION_SDIR_SO)

bootstrap:
	@mkdir -p $(TARGET) $(LAYOUTTARGET) $(COMPRESSIONTARGET)

clean: 
	@rm -rf $(BIN_DRIVER) $(COMPRESSIONTARGET)/* $(LAYOUTTARGET)/*
