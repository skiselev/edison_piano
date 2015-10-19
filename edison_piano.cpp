/******************************************************************************
* edison_piano.cpp
* Control pianobar Pandora player using Edison OLED Block
* Author: Sergey Kiselev <sergey.kiselev@intel.com>
******************************************************************************/

#include <iostream>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <upm/grove.h>
#include <upm/eboled.h>

#define MAX_STATIONS 64

using namespace std;

// Function prototypes:
int find_next_message(char **msg_ptr);
char *copy_message(char *msg_ptr, char *dst_ptr);
char *copy_time(char *msg_ptr, char *dst_ptr);
void startPianobar();
ssize_t readFromPianobar(void *readbuffer, size_t count);
void startScreen(upm::EBOLED* display);

// Global variables:
// pianobar file descriptors:
// pianobar_in - input to the control process (output of the pianobar)
// pianobar_out - output of the control process (input to the pianobar)
int pianobar_in, pianobar_out;
enum msg_type {
	MSG_INFO,
	MSG_TIME,
	MSG_STATION,
	MSG_SONG,
	MSG_ARTIST,
	MSG_ALBUM,
	MSG_OTHER,
	MSG_RATING,
	MSG_LIST,
	MSG_SEL_STATION,
	MSG_NONE
};
enum ui_mode {
	MODE_STARTING,
	MODE_PLAYING,
	MODE_SEL_STATION
};

int main(int argc, char * argv[])
{
	ssize_t nbytes;
	char readbuffer[1024], *msg_ptr;
	char update_display;
	int stations_count = 0, station_num, current_station = 0;
	char stations_list[MAX_STATIONS][64];
	char info[64], playtime[64], station[64], song[64];
	char artist[64], album[64];
	char cmd[16];
	int cmd_len;
	int msg_type;
	int btn_left_pressed = 0, btn_right_pressed = 0;
	int bnt_sel_pressed = 0;
	int mode = MODE_STARTING;

	// check that we are running on Edison
	mraa_platform_t platform = mraa_get_platform_type();
	if (platform != MRAA_INTEL_EDISON_FAB_C) {
		std::cerr << "Unsupported platform, exiting" << std::endl;
		return MRAA_ERROR_INVALID_PLATFORM;
	}
	
	upm::GroveButton* button_up = new upm::GroveButton(46);
	upm::GroveButton* button_down = new upm::GroveButton(31);
	upm::GroveButton* button_left = new upm::GroveButton(15);
	upm::GroveButton* button_right = new upm::GroveButton(45);
	upm::GroveButton* button_select = new upm::GroveButton(33);
	upm::GroveButton* button_a = new upm::GroveButton(47);
	upm::GroveButton* button_b = new upm::GroveButton(32);

        // use default connection settings for OLED
	upm::EBOLED* display = new upm::EBOLED();
	if (display == NULL) {
		std::cerr << "OLED initialization failed" << std::endl;
		return 1;
	}	
	
	startScreen(display);

	startPianobar();

	while (1) {
		update_display = 0;
		nbytes = readFromPianobar(readbuffer, sizeof(readbuffer) - 1);
		// check for read errors
		if (-1 == nbytes)
			break;

		readbuffer[nbytes] = '\0';
		msg_ptr = readbuffer;

		while (MSG_NONE != (msg_type = find_next_message(&msg_ptr))) {
			switch (msg_type) {
			case MSG_INFO:
				msg_ptr = copy_message(msg_ptr, info);
				update_display = 1;
				break;
			case MSG_TIME:
				mode = MODE_PLAYING;
				msg_ptr = copy_time(msg_ptr, playtime);
				update_display = 1;
				break;
			case MSG_STATION:
				msg_ptr = copy_message(msg_ptr, station);
				update_display = 1;
				break;
			case MSG_SONG:
				msg_ptr = copy_message(msg_ptr, song);
				update_display = 1;
				break;
			case MSG_ARTIST:
				msg_ptr = copy_message(msg_ptr, artist);
				update_display = 1;
				break;
			case MSG_ALBUM:
				msg_ptr = copy_message(msg_ptr, album);
				update_display = 1;
				break;
			case MSG_LIST:
				station_num = atoi(msg_ptr);
				msg_ptr += 8;
				msg_ptr = copy_message(msg_ptr,
						stations_list[station_num]);
				if (!strcmp(stations_list[station_num],
						station))
					current_station = station_num;
				if (stations_count < station_num + 1)
					stations_count = station_num + 1;
				break;
			case MSG_SEL_STATION:
				mode = MODE_SEL_STATION;
				update_display = 1;
				break;
			}
		}

		// process buttons
			
		if (button_up->value() == 0) {
			if (MODE_PLAYING == mode) {
				write(pianobar_out, ")", 1);
			} else if (MODE_SEL_STATION == mode) {
				if (current_station > 0) {
					current_station--;
					update_display = 1;
				}
			}
		}
		if (button_down->value() == 0) {
			if (MODE_PLAYING == mode) {
				write(pianobar_out, "(", 1);
			} else if (MODE_SEL_STATION == mode) {
				if (current_station + 1 < stations_count) {
					current_station++;
					update_display = 1;
				}
			}
		}
		if (button_right->value() == 0 && !btn_right_pressed) {
			if (MODE_PLAYING == mode) {
				write(pianobar_out, "n", 1);
			} else if (MODE_SEL_STATION == mode) {
				write(pianobar_out, "\n", 1);
			}
			btn_right_pressed = 1;
		}
		if (button_right->value() == 1) {
			btn_right_pressed = 0;
		}
		if (button_left->value() == 0 && !btn_left_pressed) {
			write(pianobar_out, "s", 1);
			btn_left_pressed = 1;
		}
		if (button_left->value() == 1) {
			btn_left_pressed = 0;
		}
		if (button_select->value() == 0 && !bnt_sel_pressed) {
			if (MODE_PLAYING == mode) {
				write(pianobar_out, "p", 1);
			} else if (MODE_SEL_STATION == mode) {
				cmd_len = snprintf(cmd, 16, "%d\n",
						current_station);
				write(pianobar_out, cmd, cmd_len);
			}
			bnt_sel_pressed = 1;
		}
		if (button_select->value() == 1) {
			bnt_sel_pressed = 0;
		}
		if (button_a->value() == 0) {
			if (MODE_PLAYING == mode) {
				write(pianobar_out, "+", 1);
			}
		}
		if (button_b->value() == 0) {
			if (MODE_PLAYING == mode) {
				write(pianobar_out, "t", 1);
			}
		}

		if (update_display) {
			if (MODE_STARTING == mode) {
				display->drawRectangleFilled(0,40,64,8,upm::COLOR_BLACK);
				display->setCursor(40,0);
				display->write(info);
			} else if (MODE_PLAYING == mode) {
				display->clearScreenBuffer();
				display->setCursor(0,0);
				display->write(station);
				display->setCursor(8,0);
				display->write(song);
				display->setCursor(16,0);
				display->write(artist);
				display->setCursor(24,0);
				display->write(artist);
				display->setCursor(40,0);
				display->write(playtime);
			} else if (MODE_SEL_STATION == mode) {
				display->clearScreenBuffer();
				display->setCursor(0,0);
				display->write(" Station?");
				display->setCursor(8,0);
				display->setTextWrap(true);
				display->write(stations_list[current_station]);
				display->setTextWrap(false);
				display->setCursor(40,0);
				display->write("Up/Down/Sel");
			}
			display->refresh();
		}

		usleep(100000);
	}

	delete display;

	return 0;
}

int find_next_message(char **msg_ptr)
{
        while (**msg_ptr != '<' && **msg_ptr != '\0')
                *msg_ptr += 1;
	if (**msg_ptr == '\0')
		return MSG_NONE;
	*msg_ptr += 1;
	switch (**msg_ptr) {
	case '\0':
		return MSG_NONE;
	case 'i':
		*msg_ptr += 1;
		return MSG_INFO;
	case 't':
		*msg_ptr += 1;
		return MSG_TIME;
	case 'n':
		*msg_ptr += 1;
		return MSG_STATION;
	case 's':
		*msg_ptr += 1;
		return MSG_SONG;
	case 'a':
		*msg_ptr += 1;
		return MSG_ARTIST;
	case 'r':
		*msg_ptr += 1;
		return MSG_RATING;
	case 'm':
		*msg_ptr += 1;
		return MSG_ALBUM;
	case 'l':
		*msg_ptr += 1;
		return MSG_LIST;
	case '?':
		*msg_ptr += 1;
		if (!strncmp(*msg_ptr, "Select station: ", 16))
			return MSG_SEL_STATION;
	}
	printf("Unknown message type %c\n", **msg_ptr);
	return MSG_OTHER;
}

char *copy_message(char *msg_ptr, char *dst_ptr)
{
	int i;
	for (i = 0; i < 63; i++) {
		if (msg_ptr[i] == '\0' || msg_ptr[i] == '>' ||
				msg_ptr[i] == '\n')
			break;
		dst_ptr[i] = msg_ptr[i];
	}
	dst_ptr[i] = '\0';
	return msg_ptr + i;
}

char *copy_time(char *msg_ptr, char *dst_ptr)
{
	int i, j = 0;
	int leading_zero = 1;
	for (i = 0; i < 63; i++) {
		if (msg_ptr[i] == '\0' || msg_ptr[i] == '>')
			break;
		// skip leading zero
		if (msg_ptr[i] == '0' && leading_zero && msg_ptr[i+1] != ':')
			continue;
		// significant digit or minutes/seconds separator
		if ((msg_ptr[i] >= '1' && msg_ptr[i] <= '9')
		    || msg_ptr[i] == ':')
			leading_zero = 0;
		if (msg_ptr[i] == '-' || msg_ptr[i] == '/')
			leading_zero = 1;
		dst_ptr[j++] = msg_ptr[i];
	}
	dst_ptr[j] = '\0';
	return msg_ptr + i;
}

// startPianobar
void startPianobar()
{
	// XXX: Child error messages should be shown on the display?
	// fd_in - input to the control process (output from pianobar)
	// fd_out - output of the control process (input to pianobar)
	int fd_in[2], fd_out[2];
	int flags;
	pid_t childpid;

	if (pipe(fd_in) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	if (pipe(fd_out) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	if((childpid = fork()) == -1)
	{
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if(childpid == 0)
	{
		// in child process
		// close the write end of the parent's output pipe
		close(fd_out[1]);
		// duplicate parent's output to stdin
		if(dup2(fd_out[0], 0) == -1) {
			perror("dup2");
			_exit(EXIT_FAILURE);
		}
		// close the read end of the parent's input pipe
		close(fd_in[0]);
		// duplicate parent's input to stdout
		if(dup2(fd_in[1], 1) == -1) {
			perror("dup2");
			_exit(EXIT_FAILURE);
		}

		// exec pianobar
		if (execl("/usr/bin/pianobar", "/usr/bin/pianobar", NULL) == -1) {
			perror("execl");
			_exit(EXIT_FAILURE);
		}
	} else {
		// in parent process
		// close the read end of the output pipe
		close(fd_out[0]);
		// close the write end of the input pipe
		close(fd_in[1]);

		pianobar_in = fd_in[0];
		pianobar_out = fd_out[1];

		if (-1 == (flags = fcntl(pianobar_in, F_GETFL, 0)))
			flags = 0;
		fcntl(pianobar_in, F_SETFL, flags | O_NONBLOCK);
	}
}

ssize_t readFromPianobar(void *readbuffer, size_t count)
{
	ssize_t nbytes;

	/* Read in a string from the pipe */
	nbytes = read(pianobar_in, readbuffer, count);
	if (-1 == nbytes) {
		if (EAGAIN == errno)
			nbytes = 0;
		else
			perror("read");
	}

	return nbytes;
}
	
void startScreen(upm::EBOLED* display)
{
	display->clear();
	display->setCursor(0,0);
	display->write("Welcome to");
	display->setCursor(8,12);
	display->write("Edison");
	display->setCursor(16,6);
	display->write("Pianobar");
	display->refresh();
}
