all:
	gcc client.c -lcrypto -o WTF
	cd server && gcc server.c -lcrypto -lpthread -o WTFserver
clean:
	rm -rf WTF
	cd server && rm -rf WTFserver
	rm WTFtest
test:
	gcc testplan.c -o WTFtest
