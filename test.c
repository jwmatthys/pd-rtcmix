/*
 * RTcmix static lib linking test
 */

#include <stdio.h>
#include "../rtcmix/RTcmix_API.h"

int main(int argc, char **argv)
{
	printf("Who cares if you listen?\n");
	RTcmix_init();
	return 0;
}

