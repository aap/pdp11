#include <stdlib.h>
#include "lodepng.h"
#include "print.h"
#include "unix.h"

#define PAGE_WIDTH  1728
#define PAGE_HEIGHT  2200

#define PAPER_MAX  (PAGE_HEIGHT * 20)
#define PAGE_MAX  (PAGE_HEIGHT * 3)

static unsigned char paper[PAGE_WIDTH * PAPER_MAX];
static unsigned char page[4 * PAGE_WIDTH * PAGE_MAX];
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
	int paper_index, page_index;
	int i, j, n;

	if(lines > PAGE_MAX)
		n = PAGE_MAX;
	else
		n = lines;

	for(j = 0; j < n; j++) {
		paper_index = PAGE_WIDTH * j;
		page_index = 4 * PAGE_WIDTH * j;
		for(i = 0; i < PAGE_WIDTH; i++) {
			page[page_index++] = paper[paper_index];
			page[page_index++] = paper[paper_index];
			page[page_index++] = paper[paper_index++];
			page[page_index++] = 0xFF;
		}
	}
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
	make_page();
	error = (int)lodepng_encode32_file(file, page, PAGE_WIDTH, lines);
	if(error == 0)
		error = lines;
	else
		error = -1;

	memmove(paper, paper + PAGE_WIDTH*lines, PAGE_WIDTH*offset);
	lines = offset;

	return error;
}
