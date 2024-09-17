.PHONY: all clean

all:
	gcc -Wall diskinfo.c -o diskinfo
	gcc -Wall disklist.c -o disklist
	gcc -Wall diskget.c -o diskget
	gcc -Wall diskput.c -o diskput


clean:
	-rm -f diskinfo
	-rm -f disklist
	-rm -f diskget
	-rm -f diskput
