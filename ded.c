#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <math.h>

struct line
{
	char *data;
	unsigned short len;
	unsigned short capacity;
};

enum modes {NORMAL, TERMINATED};

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

	enum modes mode = NORMAL;

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

			lines = realloc(lines, sizeof(struct line*) * lines_capacity);
			if(lines == NULL)
			{
				fprintf(stderr, "Failed to allocate more memory for the lines buffer.\n");
				free(file_buffer);
				return 1;
			}
		}

		// Copy line into lines buffer
		unsigned short required_size = strlen(token) + 1; // +1 for null terminator

		lines[lines_len] = malloc(sizeof(struct line));  
		struct line *current_line = lines[lines_len];

		// default allocation style for a line
		if(required_size >= 512)
			current_line->data = malloc(sizeof(char) * (required_size + 256));
		else
			current_line->data = malloc(sizeof(char) * 512);

		current_line->len = required_size;
		current_line->capacity = 512;

		// copy line into memory buffer
		strcpy(current_line->data, token);

		// increment array counters
		lines_len++;

		token = strtok(NULL, "\n");
	}

	// from here on out, we do not use the file buffer. We modify the lines directly.
	free(file_buffer);

	// Because of line numbers, top left corner is NOT the origin (0, 0). 
	// Instead, the origin is (line_number_max_digits + 1, 0)
	unsigned short cursor_x = 0;           // x
	unsigned long current_line_number = 0; // y

	// initialize ncurses
	initscr();
	cbreak();             // line buffering disabled
	noecho();             // don't echo while we do getch
	keypad(stdscr, TRUE); // enable special keys
	
	// Get window sizing
	int rows, cols;
	getmaxyx(stdscr, rows, cols); // for some reason, this doesn't need pointers

	// Setup ncurses to use colors in the terminal.
	start_color(); // enable color support
	init_pair(1, COLOR_YELLOW, A_NORMAL);

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
		unsigned short current_line_number_digits = (unsigned short) ceil(log10(i)) + 1;
		unsigned short difference = line_number_max_digits - current_line_number_digits;

		// print leading spaces
		for(int x = 0; x < difference; x++)
			printw(" ");

		// print line number digits and a separation space
		printw("%lu ", i);

		// restore color to normal
		attroff(COLOR_PAIR(1));
		refresh();

		for(unsigned short j = 0; j < lines[i]->len; j++)
		{
			char current_char = lines[i]->data[j];
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
				cursor_x = line_number_max_digits + 1;
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
			case KEY_ENTER:
				current_line++;
				if(lines_len >= lines_capacity)
				{
					lines_capacity += 256;
					lines = realloc(lines, sizeof(struct line*) * lines_capacity);
					if(lines == NULL)
					{
						fprintf(stderr, "Failed to allocate more memory for the lines buffer after adding newline (lines_len = %lu, lines_capacity = %lu).\n", lines_len, lines_capacity);
						return 1;
					}
				}
				// if adding new line at end of document, OK
				if(current_line_number == lines_len - 1)
				{
					current_line_number++;
					lines[lines_len] = malloc(sizeof(struct line));
				}
				// otherwise, all following lines must be shifted over (expensive)
				else
				{
					// case: cursor was in the middle of the text so the new line inherits 
					//       some of the text

					// case: cursor was at the end of the text, so the new line inherits nothing
				}

				// finally, ensure that new line number is still being displayed.
				attron(COLOR_PAIR(1));
				unsigned short current_line_number_digits = (unsigned short) ceil(log10(i)) + 1;
				unsigned short difference = line_number_max_digits - current_line_number_digits;
				for(int i = 0; i < difference; i++)
				{
					printw(" ");
				}
				printw("%lu", current_line_number);
				printw(" ");
				attroff(COLOR_PAIR(1)); 
				cursor_x = line_number_max_digits + 2; 
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
				if(cursor_x == line_number_max_digits + 2 && current_line_number == 0)
					break;

				// go up to previous line and set cursor to end of that line 
				else if(cursor_x == line_number_max_digits + 2 && current_line_number != 0)
				{
					if(current_line_number - 1 < 0)
						break;
	
					current_line_number--;
					cursor_x = lines[current_line_number]->len - 1;
					move(current_line_number, cursor_x); // for some reason this uses (y, x)
				}
				else // just move back 1 position
				{
					cursor_x--;
					move(current_line_number, cursor_x);
				}
				break;

			case 27: // if ESC is pressed
				mode = TERMINATED; 
				break;

			default: // add to line at current index
				struct line *current_line = lines[current_line_number];
				if(current_line->len >= current_line->capacity)
				{
					current_line->capacity += 256;
					current_line = realloc(current_line, sizeof(char) * current_line->capacity);
					if(current_line == NULL)
					{
						fprintf(stderr, "Failed to allocate more memory for line %lu.\n", current_line_number);
						for(int i = 0; i < lines_len; i++)
						{
							if(i == current_line_number)
								continue;

							free(lines[lines_len]->data);
							free(lines[lines_len]);
						}
						free(lines);
						return 1;
					}
				}
				
				current_line->data[cursor_x - (line_number_max_digits - 2)] = c; 
				cursor_x++;
				break;
		}
		if(c != KEY_BACKSPACE)
			printw("%c", c);
		refresh();
	}

	// terminated, save file
	file_buffer[file_buffer_len] = '\0';	
	SOURCE_FILE = fopen(argv[1], "w");
	for(unsigned long i = 0; i < lines_len; i++)
		fprintf(SOURCE_FILE, "%s\n", lines[i]->data);

	fclose(SOURCE_FILE);

	endwin(); // restore terminal to normal mode
	printf("Got input as \"%s\"\n", file_buffer);
	free(file_buffer);
	return 0;
}
