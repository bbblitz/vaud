/*
	vaud.c
	A console based audio visualizer for JACK
	Copyright (C) 2014 Alexander M. Pickering

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	A big thank you goes to Nicholas J. Humfrey, whos jack meter
	was modified to make this. You can view the project here:
	http://www.aelius.com/njh/jackmeter/
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ncurses.h>
#include <jack/jack.h>
#include <getopt.h>
#include "config.h"

/*Stuff to get from the settings file*/

float bias = 1.0f;
float peak = 0.0f;
jack_default_audio_sample_t *data;
jack_nframes_t dframes;
int dpeak = 0;
int dtime = 0;
int decay_len;
int set = 0;
jack_port_t *input_port = NULL;
jack_client_t *client = NULL;

WINDOW *create_newwin(int height, int width, int starty, int startx);

/* Read and reset the recent peak sample */
static float read_peak()
{
	float tmp = peak;
	peak = 0.0f;

	return tmp;
}
/* Callback called by JACK when audio is available.
   Stores value of peak sample */
static int process_peak(jack_nframes_t nframes, void *arg)
{
	dframes = nframes;
	jack_default_audio_sample_t *in;
	unsigned int i;


	/* just incase the port isn't registered yet */
	if (input_port == NULL) {
		return 0;
	}


	/* get the audio samples, and find the peak sample */
	in = (jack_default_audio_sample_t *) jack_port_get_buffer(input_port, nframes);
	for (i = 0; i < nframes; i++) {
		const float s = fabs(in[i]);
		if (s > peak) {
			peak = s;
		}
	}
	data = in;

	return 0;
}


/* Close down JACK when exiting */
static void cleanup()
{
	const char **all_ports;
	unsigned int i;

	fprintf(stderr,"cleanup()\n");

	if (input_port != NULL ) {

		all_ports = jack_port_get_all_connections(client, input_port);

		for (i=0; all_ports && all_ports[i]; i++) {
			jack_disconnect(client, all_ports[i], jack_port_name(input_port));
		}
	}

	/* Leave the jack graph */
	jack_client_close(client);

}


/* Connect the chosen port to ours */
static void connect_port(jack_client_t *client, char *port_name)
{
	jack_port_t *port;

	// Get the port we are connecting to
	port = jack_port_by_name(client, port_name);
	if (port == NULL) {
		fprintf(stderr, "Can't find port '%s'\n", port_name);
		exit(1);
	}

	// Connect the port to our input port
	fprintf(stderr,"Connecting '%s' to '%s'...\n", jack_port_name(port), jack_port_name(input_port));
	if (jack_connect(client, jack_port_name(port), jack_port_name(input_port))) {
		fprintf(stderr, "Cannot connect port '%s' to '%s'\n", jack_port_name(port), jack_port_name(input_port));
		exit(1);
	}
}


/* Sleep for a fraction of a second */
static int fsleep( float secs )
{

//#ifdef HAVE_USLEEP
	return usleep( secs * 1000000 );
//#endif
}


/* Display how to use this program */
static int usage( const char * progname )
{
	fprintf(stderr, "Visual Audio Version - %s\n\n", VERSION);
	fprintf(stderr, "Usage %s [-f freqency] [-m meter] [-n] [<port>, ...]\n\n", progname);
	fprintf(stderr, "where  -f      is how often to update the meter per second [8]\n");
 	fprintf(stderr, "       -g      is the number of the meter (0-2)\n");
	fprintf(stderr, "       -n      changes mode to output meter level as number in decibels\n");
	fprintf(stderr, "       <port>  the port(s) to monitor (multiple ports are mixed)\n");
	exit(1);
}

/*Start the ncurses stuff
 */
void start_ncurses()
{
	if(COLORFUL && !has_colors) //Settings tell us to be colorful,
				    //but terminal does not support colors.
	{
		printf("Your terminal does not support colors! Chage \"COLORFUL\" in config.h to 0 and rebuild.");
		exit(1);
	}
	initscr(); //Start ncurses
	curs_set(0); //Hide cursor
	if(COLORFUL)
	{
		start_color(); //Tell ncurses to be colorufl!
		init_pair(1, BORCOLORFG, BORCOLORBG); //Color the border
		init_pair(2, BARCOLORFG, BARCOLORBG); //Color the bars
		init_pair(3, BLANKCOLORBG, BLANKCOLORFG); //Color the blank space	
	}
}

/* Creates a new NCurses window
 *int height	: the height of the window to make
 *int width 	: the width of the window to make
 *int startx	: the column to start the window on
 *int starty	: the row to start the window on
 */
WINDOW *create_newwin(int height, int width, int startx, int starty)
{       WINDOW *local_win;
	if(COLORFUL)
		attron(COLOR_PAIR(1));
        local_win = newwin(height, width, starty, startx);
        box(local_win, 0 , 0);
        wrefresh(local_win);
	if(COLORFUL)
		attroff(COLOR_PAIR(1));

        return local_win;
}

void display_centerBars()
{
	int width,height,startx,starty,i=0;
	WINDOW *my_win;

	/*Get width and height of console, make border*/
	getmaxyx(stdscr,height,width);
	my_win = create_newwin(height,width,startx,starty);
	
	/*Figure out how which nodes we can safely get*/
	int space = ((unsigned int) dframes) / width;
	//mvprintw(10,10,"Space is %i",space);
	//refresh();
	if(space > 0)
	{
		for(i=1;i<width-1;i+=1)
		{
			float samp = fabs(data[space * i]); //get the waveform data
			float dec;
			//mvprintw(height/2,i,BAR);
			int j;
			for(j=0;j<samp*(height/2);j++)
			{
				if(COLORFUL)
					attron(COLOR_PAIR(2));
				mvprintw((height/2)+j,i,BAR);
				mvprintw((height/2)-j,i,BAR);
				if(COLORFUL)
					attroff(COLOR_PAIR(2));
					
			}
			for(j=samp*(height/2)+1;j<(height/2);j++)
			{
				if(COLORFUL)
					attron(COLOR_PAIR(3));
				mvprintw((height/2)+j,i,BLANK);
				mvprintw((height/2)-j,i,BLANK);
				if(COLORFUL)
					attroff(COLOR_PAIR(3));
			}
		}
	}
	refresh();
}


void display_rmeter()
{
	int width,height,startx,starty,i=0;
	WINDOW *my_win;

	/*Get width and height of console, make border*/
	getmaxyx(stdscr,height,width);
	my_win = create_newwin(height,width,startx,starty);
	
	/*Figure out how which nodes we can safely get*/
	int space = ((unsigned int) dframes) / width;
	if(space > 0)
	{
		for(i=1;i<width-1;i+=1)
		{
			float samp = fabs(data[space * i]); //get the waveform data
			float dec;
			int j;
			for(j=height-2;j>((1-samp)*height);j--)
			{
				if(COLORFUL)
					attron(COLOR_PAIR(2));
				mvprintw(j,i,BAR);
				if(COLORFUL)
					attroff(COLOR_PAIR(2));
					
			}
			for(j=((1-samp)*height-2);j>0;j--)
			{
				if(COLORFUL)
					attron(COLOR_PAIR(3));
				mvprintw(j,i,BLANK);
				if(COLORFUL)
					attroff(COLOR_PAIR(3));
			}
		}
	}
	refresh();
}

/* Displays the graph
 */
void display_meter()
{
	int width,height,startx,starty,i=0;
	WINDOW *my_win;

	/*Get width and height of console, make border*/
	getmaxyx(stdscr,height,width);
	my_win = create_newwin(height,width,startx,starty);
	
	/*Figure out how which nodes we can safely get*/
	int space = ((unsigned int) dframes) / width;
	if(space > 0)
	{
		for(i=1;i<width-1;i+=1)
		{
			float samp = fabs(data[space * i]); //get the waveform data
			float dec;
			int j;
			for(j=1;j<((samp)*height);j++)
			{
				if(COLORFUL)
					attron(COLOR_PAIR(2));
				mvprintw(j,i,BAR);
				if(COLORFUL)
					attroff(COLOR_PAIR(2));
					
			}
			for(j=((samp)*height)+1;j<height-1;j++)
			{
				if(COLORFUL)
					attron(COLOR_PAIR(3));
				mvprintw(j,i,BLANK);
				if(COLORFUL)
					attroff(COLOR_PAIR(3));
			}
		}
	}
	refresh();
}


int main(int argc, char *argv[])
{
	int console_width = 79;
	jack_status_t status;
	int running = 1;
	float ref_lev;
	int decibels_mode = 0;
	int rate = 8;
	int opt;
	int graph = 1;

	// Make STDOUT unbuffered
	setbuf(stdout, NULL);

	while ((opt = getopt(argc, argv, "f:g:n")) != -1) {
		switch (opt) {
			case 'g':
				graph = atoi(optarg);
				break;
			case 'f':
				rate = atoi(optarg);
				fprintf(stderr,"Updates per second: %d\n", rate);
				break;
			case 'n':
				decibels_mode = 1;
				break;
			default:
				/* Show usage/version information */
				usage( argv[0] );
				break;
		}
	}



	// Register with Jack
	if ((client = jack_client_open("meter", JackNullOption, &status)) == 0) {
		fprintf(stderr, "Failed to start jack client: %d\n", status);
		exit(1);
	}
	fprintf(stderr,"Registering as '%s'.\n", jack_get_client_name( client ) );

	// Create our input port
	if (!(input_port = jack_port_register(client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0))) {
		fprintf(stderr, "Cannot register input port 'meter'.\n");
		exit(1);
	}

	// Register the cleanup function to be called when program exits
	atexit( cleanup );

	// Register the peak signal callback
	jack_set_process_callback(client, process_peak, 0);


	if (jack_activate(client)) {
		fprintf(stderr, "Cannot activate client.\n");
		exit(1);
	}


	// Connect our port to specified port(s)
	if (argc > optind) {
		while (argc > optind) {
			connect_port( client, argv[ optind ] );
			optind++;
		}
	} else {
		fprintf(stderr,"Meter is not connected to a port.\n");
	}

	// Calculate the decay length (should be 1600ms)
	decay_len = (int)(1.6f / (1.0f/rate));


	// Display the scale
	if (decibels_mode==0) {
		start_ncurses();
	}
	while (running) {
		float db = 20.0f * log10f(read_peak() * bias);

		if (decibels_mode==1) {
			printf("%1.1f\n", db);
		} else {
			switch(graph)
			{	
				case 0 : display_rmeter();break;
				case 1 : display_meter();break;
				case 2 : display_centerBars();break;
			}
		}

		fsleep( 1.0f/rate );
	}
	endwin();
	return 0;
}

