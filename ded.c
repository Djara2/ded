#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>

int main()
{
	enum modes {NORMAL, NORMAL_COMMAND, INSERT, TERMINATED, COMMAND_EVALUATE};
	enum modes mode = NORMAL;

	unsigned short input_buffer_len = 0;
	unsigned short input_buffer_capacity = 128;
	char *input_buffer = malloc(sizeof(char) * 128);

	unsigned short command_buffer_len = 0;
	unsigned short command_buffer_capacity = 128;
	char *command_buffer = malloc(sizeof(char) * 128);
	
	char c;
	// initialize ncurses
	initscr();
	cbreak();             // line buffering disabled
	noecho();             // don't echo while we do getch
	keypad(stdscr, TRUE); // enable special keys

	printf("Begin typing:\n> ");
	while(mode != TERMINATED)  // loop so long as we do not read ESC key
	{ 
		c = getch();
		if(mode == INSERT)
		{
			if(input_buffer_len >= input_buffer_capacity)
			{
				input_buffer_capacity *= 2;
				input_buffer = realloc(input_buffer, sizeof(char) * input_buffer_capacity);
				if(input_buffer == NULL)
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
					if(input_buffer_len <= 0)
						break;

					// move cursor one position to the left
					move(getcury(stdscr), getcurx(stdscr) - 1);

					// replace character with space
					addch(' ');

					// move cursor back to original position
					move(getcury(stdscr), getcurx(stdscr) - 1);

					// decrement buffer that we have been reading
					input_buffer[input_buffer_len - 1] = '\0';
					input_buffer_len--;
					break;
				default:
					input_buffer[input_buffer_len] = c;
					input_buffer_len++;
					break;
			}
			if(c != 7)
				printw("%c", c);
		}
		else if(mode == NORMAL) // NORMAL mode
		{
			switch(c)
			{
				case 'i':
					mode = INSERT;
					break;
				case ':':
					mode = NORMAL_COMMAND;
					break;
				default:
					break;	
			}
		}
		else if(mode == NORMAL_COMMAND) // NORMAL_COMMAND mode (mode for writing commands)
		{
			switch(c)
			{
				case 27: // escape key returns to normal mode
					mode = NORMAL;
					break;
				case '\n':
					mode = COMMAND_EVALUATE;
					break;
				default:
					if(command_buffer_len >= command_buffer_capacity)
					{
						command_buffer_capacity *= 2;
						command_buffer = realloc(command_buffer, sizeof(char) * command_buffer_capacity);
						if(command_buffer == NULL)
						{
							fprintf(stderr, "Failed to allocate more memory for command buffer.\n");
							free(input_buffer);
							return 1;
						}
					}
					command_buffer[command_buffer_len] = c;
					command_buffer_len++;
			}
		}
		else // COMMAND_EVALUATE
		{
			if(strcmp(command_buffer, "wq") == 0)
				mode = TERMINATED;

			// reset command buffer
			command_buffer_len = 0;
			command_buffer[0] = '\0';

			// go back to normal mode
			mode = NORMAL;
		}
		refresh();
	}

	endwin(); // restore terminal to normal mode
	printf("Got input as \"%s\"\n", input_buffer);
	free(input_buffer);
	return 0;
}
