#include <stdlib.h>
#include "lodepng.h"
#include "print.h"
#include "unix.h"

#define PAGE_WIDTH  1728
#define PAGE_HEIGHT  2200

#define PAPER_MAX  (PAGE_HEIGHT * 20)
#define PAGE_MAX  (PAGE_HEIGHT * 3)

extern void meatball(int width, int height, unsigned char *paper, unsigned char *image, unsigned char *page);

static unsigned char paper[PAGE_WIDTH * PAPER_MAX];
static unsigned char page[4 * PAGE_WIDTH * PAGE_MAX];
static unsigned char image[4 * 4 * PAGE_WIDTH * 4 * PAGE_MAX];
static int lines, position;

void print_start()
{
	lines = 0;
}

void print_line()
{
	if(lines >= PAPER_MAX)
		return;

	lines++;

	// Clear the line
	position = PAGE_WIDTH;
	do {
		--position;
		print_dot(0);
		--position;
	} while(position > 0);
}

void print_dot(int bw)
{
	if(lines >= PAPER_MAX)
		return;
	if(position >= PAGE_WIDTH)
		return;

	paper[(lines-1) * PAGE_WIDTH + position] = bw ? 0x00 : 0xFF;
	position++;
}

static void make_page(void)
{
	int n = lines;
	if(n > PAGE_MAX)
		n = PAGE_MAX;
	meatball(PAGE_WIDTH, n, paper, image, page);
}

int print_finish(int offset, char *file, size_t n)
{
	char time[100];
	int error;

	if(lines <= offset + 2) {
		*file = 0;
		return 0;
	}
	lines -= offset;

	timestamp(time, sizeof time);
	snprintf(file, n, "xgp-%s.png", time);
	if(fork()) {
		error = lines;
	} else {
		make_page();
		error = lodepng_encode32_file(file, page, PAGE_WIDTH, lines);
		exit(error == 0 ? 0 : 1);
	}

	memmove(paper, paper + PAGE_WIDTH*lines, PAGE_WIDTH*offset);
	lines = offset;

	return error;
}
