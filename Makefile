all: small full

full:
	cc -UESN -o no-esn test.c
	cc -DESN -o esn test.c
	./no-esn
	./esn
small:
	cc -UESN -DTDB_REPLAYMAX="(32+32)" -o no-esn-small test.c
	cc -DESN -DTDB_REPLAYMAX="(32+32)" -o esn-small test.c
	./no-esn-small
	./esn-small

clean:
	rm -f esn esn-small no-esn no-esn-small
