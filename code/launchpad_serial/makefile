NAME            = main
OBJECTS         = main.o
CPU             = msp430g2553

CFLAGS          = -mmcu=${CPU} -O2 -Wall -g
CC              = msp430-gcc

.PHONY: all FORCE clean program

#all should be the first target. it's built when make is run without args
all: ${NAME}.elf ${NAME}.a43 ${NAME}.lst

#additional rules for files
${NAME}.elf: ${OBJECTS}
	${CC} -mmcu=${CPU} -o $@ ${OBJECTS}

${NAME}.a43: ${NAME}.elf
	msp430-objcopy -O ihex $^ $@

${NAME}.lst: ${NAME}.elf
	msp430-objdump -dSt $^ >$@

program: all
	mspdebug rf2500 "prog ${NAME}.elf"

clean:
	rm -f ${NAME}.elf ${NAME}.a43 ${NAME}.lst ${OBJECTS}

#dummy target as dependecy if something has to be build everytime
FORCE:

#project dependencies
main.o: main.c 
