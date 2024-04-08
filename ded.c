#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "Provide a file to create.\n");
		return 1;
	}

	FILE *SOURCE_FILE = fopen(argv[1], "r");

	if(SOURCE_FILE == NULL)
	{
		fprintf(stderr, "Failed to open \"%s\" for reading and writing.\n", argv[1]);
		return 1;
	}

	enum modes {NORMAL, NORMAL_COMMAND, INSERT, TERMINATED, COMMAND_EVALUATE};
	enum modes mode = NORMAL;

	// store file contents for modification. At first, it stores 1,000,000 bytes (characters) (1 megabyte).
	// It is unlikely that more memory should be needed.
	unsigned long file_buffer_len = 0;
	unsigned long file_buffer_capacity = 1000000;
	char *file_buffer = malloc(sizeof(char) * 1000000);

	unsigned short command_buffer_len = 0;
	unsigned short command_buffer_capacity = 128;
	char *command_buffer = malloc(sizeof(char) * 128);
	
	char c;

	// read contents into file buffer
	while((c = fgetc(SOURCE_FILE)) != EOF)
	{
		if(file_buffer_len + 1 >= file_buffer_capacity)
		{
			file_buffer_capacity += 1000000;
			file_buffer = realloc(file_buffer, sizeof(char) * file_buffer_capacity); 
			if(file_buffer == NULL)
			{
				fprintf(stderr, "Failed to allocate more memory for reading of file.\n");
				return 1;
			}
		}
		file_buffer[file_buffer_len] = c;
		file_buffer_len++;
	}
	fclose(SOURCE_FILE);

	// initialize ncurses
	initscr();
	cbreak();             // line buffering disabled
	noecho();             // don't echo while we do getch
	keypad(stdscr, TRUE); // enable special keys

	// display file contents
	for(unsigned long i = 0; i < file_buffer_len; i++)
	{
		printw("%c", file_buffer[i]);
		refresh();
	}

	while(mode != TERMINATED)  // loop so long as we do not read ESC key
	{ 
		c = getch();
		if(file_buffer_len + 1 >= file_buffer_capacity)
		{
			file_buffer_capacity *= 2;
			file_buffer = realloc(file_buffer, sizeof(char) * file_buffer_capacity);
			if(file_buffer == NULL)
			{
				fprintf(stderr, "Failed to allocate more memory for input buffer.\n");
				return 1;
			}
		}

		switch(c)
		{
			case '\t':
				printw("\nDo you want autocorrect options?\n> ");
				break;
			case 7: // backspace
				if(file_buffer_len <= 0)
					break;

				// move cursor one position to the left
				move(getcury(stdscr), getcurx(stdscr) - 1);

				// replace character with space
				addch(' ');

				// move cursor back to original position
				move(getcury(stdscr), getcurx(stdscr) - 1);

				// decrement buffer that we have been reading
				file_buffer[file_buffer_len - 1] = '\0';
				file_buffer_len--;
				break;
			case 27:
				mode = TERMINATED; 
				break;
			default:
				file_buffer[file_buffer_len] = c;
				file_buffer_len++;
				break;
		}
		if(c != 7)
			printw("%c", c);
		refresh();
	}

	// terminated, save file
	file_buffer[file_buffer_len] = '\0';	
	SOURCE_FILE = fopen(argv[1], "w");
	fprintf(SOURCE_FILE, "%s", file_buffer);
	fclose(SOURCE_FILE);

	endwin(); // restore terminal to normal mode
	printf("Got input as \"%s\"\n", file_buffer);
	free(file_buffer);
	return 0;
}
