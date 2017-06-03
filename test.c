/*
 * RTcmix static lib linking test
 */

#include <stdio.h>
#include "../rtcmix/RTcmix_API.h"

void post(const char *fmt, ...) {}; // ignore the post errors from message.c

int main(int argc, char **argv)
{
	printf("Who cares if you listen?\n");
	RTcmix_init();
	return 0;
}

