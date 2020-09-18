#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <iostream>

int main(int argc, char** argv)
{
	char command[100];
	printf("--------------------\nTest Case 1:\n");
	strcpy(command, "cd server && ./WTFserver");
	system(command);

	printf("--------------------\nTest Case 2:\n");
	strcpy(command, "cd server && ./WTFserver 1");
	system(command);

	printf("--------------------\nTest Case 3:\n");
	strcpy(command, "cd server && ./WTFserver 2020&");
	system(command);
	strcpy(command, "./WTF configure $HOSTNAME 2020");
	system(command);
	strcpy(command, "./WTF create testProject");
	system(command);

	printf("--------------------\nTest Case 4:\n");
	//strcpy(command, "./WTF configure $HOSTNAME 2021");
	//system(command);
	strcpy(command, "./WTF create test2");
	system(command);

	printf("---------------------\nTest Case 5:\n");
	strcpy(command, "cd testProject && touch file.txt");
	system(command);
	//strcpy(command, "./WTF configure $HOSTNAME 2020");
	//system(command);
	strcpy(command, "./WTF add testProject file.txt");
	system(command);

	printf("---------------------\nTest Case 6:\n");
	//strcpy(command, "./WTF configure $HOSTNAME 2020");
	//system(command);
	strcpy(command, "./WTF remove testProject file.txt");
	system(command);

	printf("---------------------\nTest Case 7:\n");
	//strcpy(command, "./WTF configure $HOSTNAME 2020");
	//system(command);
	strcpy(command, "./WTF destroy testProject");
	system(command);

	return 0;
}
