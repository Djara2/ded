#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <math.h>

struct line
{
	char *contents;
	unsigned short contents_len;
	unsigned short contents_capacity;
}

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

	// store file contents for modification. At first, it stores 1,000,000 bytes (characters) (1 megabyte).
	// It is unlikely that more memory should be needed.
	unsigned long file_buffer_len = 0;
	unsigned long file_buffer_capacity = 1000000;
	char *file_buffer = malloc(sizeof(char) * 1000000);

	unsigned short command_buffer_len = 0;
	unsigned short command_buffer_capacity = 128;
	char *command_buffer = malloc(sizeof(char) * 128);
	
	int c;
	unsigned long lines_len = 0;
	unsigned long lines_capacity = 1024;
	struct line **lines = malloc(sizeof(struct line*) * lines_capacity);

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

	// tokenize file so we have all lines and know how many line numbers to display
	// in yellow.
	char *token = strtok(file_buffer, "\n");
	while(token != NULL)
	{
		if(lines_len >= lines_capacity)
		{
			lines_capacity += 1024;

			lines = realloc(lines, sizeof(char*) * lines_capacity);
			line_lengths = realloc(line_lengths, sizeof(unsigned short) * lines_capacity);
			if(lines == NULL)
			{
				fprintf(stderr, "Failed to allocate more memory for the lines buffer.\n");
				free(file_buffer);
				free(line_lengths);
				return 1;
			}
			if(line_lengths == NULL)
			{
				fprintf(stderr, "Failed to allocate more memory for the line lengths buffer.\n");
				free(file_buffer);
				free(lines);
				return 1;
			}
		}
		// always ensure +1 bytes so that the null terminator and newline can be reinserted at ease.
		unsigned short required_size = strlen(token) + 1;

		// create memory for line. Record how big that line is in the line_lengths buffer
		lines[lines_len] = malloc(sizeof(char) * required_size);
		line_lengths[lines_len] = required_size;

		// copy line into memory buffer
		strcpy(lines[lines_len], token);

		// increment array counters
		lines_len++;

		token = strtok(NULL, "\n");
	}

	// from here on out, we do not use the file buffer. We modify the lines directly.
	free(file_buffer);

	// top left corner is origin (0, 0). 
	unsigned short cursor_x = 0;    // x
	unsigned long current_line = 0; // y

	// initialize ncurses
	initscr();
	cbreak();             // line buffering disabled
	noecho();             // don't echo while we do getch
	keypad(stdscr, TRUE); // enable special keys
	
	// window sizing
	int rows, cols;
	getmaxyx(stdscr, rows, cols); // for some reason, this doesn't need pointers

	// color
	start_color(); // enable color support
	init_pair(1, COLOR_YELLOW, A_NORM);

	// formula: ceil(log(lines_len, 10)) + 2
	// - for example, to display line number 1000, you need 4 digits.
	//   log(1000, 10) = 3, so we add 1 to show all digits.
	unsigned short line_number_max_digits = (unsigned short) ceil(log10(lines_len)) + 1;


	// display file contents
	for(unsigned long i = 0; i < lines_len; i++)
	{
		// first, display line number
		// print first line number in yellow
		attron(COLOR_PAIR(1));

		// determine how many leading spaces
		unsigned short current_line_digits = (unsigned short) ceil(log10(i)) + 1;
		unsigned short difference = line_number_max_digits - current_line_digits;

		// print leading spaces
		for(int x = 0; x < difference; x++)
			printw(" ");

		// print line number digits and a separation space
		printw("%lu ", i);

		// restore color to normal
		attroff(COLOR_PAIR(1));
		refresh();

		for(unsigned short j = 0; j < strlen(lines[i]); j++)
		{
			char current_char = lines[i][j];
			printw("%c", current_char);
			cursor_x++;
			// if line requires more columns than window has available, go but do not 
			// print new line number. Instead, just print whitespace equal to the 
			// maximum amount of digits for the lines we have
			if(cursor_x >= cols)
			{
				for(int blankc = 0; blankc < line_number_max_digits; blankc++)
					printw(" ");
				printw(" ");
			}
		}
		printw("\n");
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
			case KEY_BACKSPACE: // backspace
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
			case KEY_LEFT:
				// do nothing if at origin
				if(cursor_x == 0 && current_line == 0)
					break;

				// go up to previous line and set cursor to end of that line 
				else if(cursor_x == 0 && current_line != 0)
				{
					if(current_line - 1 < 0)
						break;
	
					current_line--;
					cursor_x = strlen(lines[current_line]) - 1;
					move(current_line, cursor_x); // for some reason this uses (y, x)
				}
				else // just move back 1 position
				{
					cursor_x--;
					move(current_line, cursor_x);
				}
				break;

			case 27:
				mode = TERMINATED; 
				break;
			default:
				if(cursor_x < 
				{
					
				}
				lines[current_line] = c;
				break;
		}
		if(c != 7)
			printw("%c", c);
		refresh();
	}

	// terminated, save file
	file_buffer[file_buffer_len] = '\0';	
	SOURCE_FILE = fopen(argv[1], "w");
	for(unsigned long i = 0; i < lines_len; i++)
		fprintf(SOURCE_FILE, "%s\n", lines[i]);

	fclose(SOURCE_FILE);

	endwin(); // restore terminal to normal mode
	printf("Got input as \"%s\"\n", file_buffer);
	free(file_buffer);
	return 0;
}
