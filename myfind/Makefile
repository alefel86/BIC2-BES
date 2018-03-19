########## Variables ##########

CC=gcc52
CFLAGS=-Wall -Wextra
CP=cp
CD=cd
MV=mv
GREP=grep
DOXYGEN=doxygen

OBJECTS=myfind.o

EXCLUDE_PATTERN=footrulewidth


########## Rules ##########

# Compile a C source code file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c $<


########## Targets ##########

# Compile and link all object files
.PHONY: all
all: myfind

myfind: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^


# Delete compilation output
.PHONY: clean
clean:
	$(RM) *.o *~ myfind


# Delete compilation output and documentation
.PHONY: distclean
distclean: clean
	$(RM) -r doc

doc: html pdf


.PHONY: html

# Create HTML documentation
html:
	$(DOXYGEN) doxygen.dcf

# Create HTML documentation and PDF file
pdf: html
	$(CD) doc/pdf && \
	$(MV) refman.tex refman_save.tex && \
	$(GREP) -v $(EXCLUDE_PATTERN) refman_save.tex > refman.tex && \
	$(RM) refman_save.tex && \
	make && \
	$(MV) refman.pdf refman.save && \
	$(RM) *.pdf *.html *.tex *.aux *.sty *.log *.eps *.out *.ind *.idx \
		*.ilg *.toc *.tps Makefile && \
	$(MV) refman.save refman.pdf
