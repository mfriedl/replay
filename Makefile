all:
	cc -UESN -o no-esn test.c
	cc -DESN -o esn test.c
	./no-esn
	./esn
clean:
	rm -f esn no-esn
