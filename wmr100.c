 /*
	This is hacked up version of the  original WMR  program created by Barnaby Gray. Its original license/copyright  is below
	Barnaby Gray's code is still used to control the packets received from LibHID . Those parts were pretty much left unaltered.
	
	New code added to log data into MySQL, provide help options, check/change running user, fork and debug modes.  
	And I also added some code to filter out numerous repeatitive data from my unit.  And I changed the signaling control around a bit.
	
	This was only tested with a simple RMS600  - Clock and Temp are the only items received back on my unit but I included tables for the other sensors of the WMR.  
	As long as the original WMR program worked with the other sensors,  then this program "should" log that data too.  Please let me know if there are any problems. 
	
	If this makes sense...
	Added Code - Copyright 2010 - Richard@Alcalde.us under GNU Public License
	
	And...
	
	* This program is free software: you can redistribute it and/or modify
	 * it under the terms of the GNU General Public License as published by
	 * the Free Software Foundation, either version 3 of the License, or
	 * (at your option) any later version.
	 *
	 * This program is distributed in the hope that it will be useful,
	 * but WITHOUT ANY WARRANTY; without even the implied warranty of
	 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 * GNU General Public License for more details.
	 *
	 * You should have received a copy of the GNU General Public License
	 * along with this program.  If not, see <http://www.gnu.org/licenses/>.


	 NOTE: Requires libhid  and ofcourse MySQL :)
	 
	 I provide limited or no support for this program - I only offer this code in the hope it may be useful for someone else. 
	 I made this for myself to log the data from my RMS unit in a practical useful manner.
	 
	Good Luck...
	
	SIMPLE START
	1. Cross fingers
	2. Run make clean; make
	3. Create a database called weather
	4. Optional but I suggest you create a user and password in SQL just for the weather database. (Google it.)
	5. run program to create tables : wmr100 -u SQLUser -p SQLpass -I
	5. run program : wmr100 -u SQLUser -p SQLpass 
	
	Still need help try: wmr100 -h 
 
 */
/**
 * Oregon Scientific WMR100/200 protocol. Tested on wmr100.
 *
 * Copyright 2009 Barnaby Gray <barnaby@pickle.me.uk>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <hid.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <getopt.h>
#include "mysql.h"
#include <getopt.h>
#include <pwd.h>
#include<setjmp.h>
#define _XOPEN_SOURCE

#define WMR100_VENDOR_ID  0x0fde
#define WMR100_PRODUCT_ID 0xca01

/* constants */
int const RECV_PACKET_LEN   = 8;
int const BUF_SIZE = 255;
unsigned char const PATHLEN = 2;
int const PATH_IN[]  = { 0xff000001, 0xff000001 };
int const PATH_OUT[] = { 0xff000001, 0xff000002 };
unsigned char const INIT_PACKET1[] = { 0x20, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00 };
unsigned char const INIT_PACKET2[] = { 0x01, 0xd0, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00 };

typedef struct _WMR {
    int pos;
    int remain;
    unsigned char* buffer;
    HIDInterface *hid;
    FILE *data_fh;
    char *data_filename;
} WMR;

#define TIMEOUT 5
#define BSIZE 4096
#define db_name "weather"
#define PASS_SIZE 100
#define VERSION_STRING ".001"


int DEBUGIT;
static void sig_catch(int);
static sigjmp_buf jmpbuffer;

WMR *wmr = NULL;

static void sig_catcher(int sig_no)
{
	siglongjmp(jmpbuffer, 1);	
	
}//signal Catch

void dump_packet(unsigned char *packet, int len)
{
    int i;

    printf("Receive packet len %d: ", len);
    for(i = 0; i < len; ++i)
	printf("%02x ", (int)packet[i]);
    printf("\n");
}

/****************************
  WMR methods
 ****************************/

WMR *wmr_new() {
    WMR *wmr = malloc(sizeof(WMR));
    wmr->remain = 0;
    wmr->buffer = malloc(BUF_SIZE);
    if (wmr->buffer == NULL) {
      free(wmr);
      return NULL;
    }
    wmr->data_fh = NULL;
    wmr->data_filename = "./data.log";
    return wmr;
}

int wmr_init(WMR *wmr) {
    hid_return ret;
    HIDInterfaceMatcher matcher = { WMR100_VENDOR_ID, WMR100_PRODUCT_ID, NULL, NULL, 0 };
    int retries;

    /* see include/debug.h for possible values */
    /*hid_set_debug(HID_DEBUG_ALL);*/
    /*hid_set_debug_stream(stderr);*/
    /* passed directly to libusb */
    /*hid_set_usb_debug(0);*/
  
    ret = hid_init();
    if (ret != HID_RET_SUCCESS) {
	fprintf(stderr, "hid_init failed with return code %d\n", ret);
	return 1;
    }

    wmr->hid = hid_new_HIDInterface();
    if (wmr->hid == 0) {
	fprintf(stderr, "hid_new_HIDInterface() failed, out of memory?\n");
	return 1;
    }

    retries = 5;
    while(retries > 0) {
        ret = hid_force_open(wmr->hid, 0, &matcher, 10);
	if (ret == HID_RET_SUCCESS) break;

	fprintf(stderr, "Open failed, sleeping 5 seconds before retrying..\n");
	sleep(5);

	--retries;
    }
    if (ret != HID_RET_SUCCESS) {
      fprintf(stderr, "hid_force_open failed with return code %d\n", ret);
      return 1;
    }
    
    ret = hid_write_identification(stdout, wmr->hid);
    if (ret != HID_RET_SUCCESS) {
	fprintf(stderr, "hid_write_identification failed with return code %d\n", ret);
	return 1;
    }

    wmr_send_packet_init(wmr);
    wmr_send_packet_ready(wmr);
    return 0;
}

int wmr_send_packet_init(WMR *wmr) {
    int ret;

    ret = hid_set_output_report(wmr->hid, PATH_IN, PATHLEN, (char*)INIT_PACKET1, sizeof(INIT_PACKET1));
    if (ret != HID_RET_SUCCESS) {
	fprintf(stderr, "hid_set_output_report failed with return code %d\n", ret);
	return;
    }
}

int wmr_send_packet_ready(WMR *wmr) {
    int ret;
    
    ret = hid_set_output_report(wmr->hid, PATH_IN, PATHLEN, (char*)INIT_PACKET2, sizeof(INIT_PACKET2));
    if (ret != HID_RET_SUCCESS) {
	fprintf(stderr, "hid_set_output_report failed with return code %d\n", ret);
	return;
    }
}

void wmr_print_state(WMR *wmr) {
    fprintf(stderr, "WMR: HID: %08x\n", wmr->hid);
}

int wmr_close(WMR *wmr) {
    hid_return ret;

    ret = hid_close(wmr->hid);
    if (ret != HID_RET_SUCCESS) {
	fprintf(stderr, "hid_close failed with return code %d\n", ret);
	return 1;
    }

    hid_delete_HIDInterface(&wmr->hid);
    wmr->hid = NULL;

    ret = hid_cleanup();
    if (ret != HID_RET_SUCCESS) {
	fprintf(stderr, "hid_cleanup failed with return code %d\n", ret);
	return 1;
    }

    if (wmr->data_fh && wmr->data_fh != stdout) {
	fclose(wmr->data_fh);
	wmr->data_fh = NULL;
    }
}

void wmr_read_packet(WMR *wmr)
{
    int ret, len, i;

    ret = hid_interrupt_read(wmr->hid,
			     USB_ENDPOINT_IN + 1,
			     (char*)wmr->buffer,
			     RECV_PACKET_LEN,
			     0);
    if (ret != HID_RET_SUCCESS) {
	fprintf(stderr, "hid_interrupt_read failed with return code %d\n", ret);
	exit(-1);
	return;
    }
    
    len = wmr->buffer[0];
    if (len > 7) len = 7; /* limit */
    wmr->pos = 1;
    wmr->remain = len;
    
    /* dump_packet(wmr->buffer + 1, wmr->remain); */
}

int wmr_read_byte(WMR *wmr)
{
    while(wmr->remain == 0) {
	wmr_read_packet(wmr);
    }
    wmr->remain--;

    return wmr->buffer[wmr->pos++];
}

int verify_checksum(unsigned char * buf, int len) {
    int i, ret = 0, chk;
    for (i = 0; i < len -2; ++i) {
	ret += buf[i];
    }
    chk = buf[len-2] + (buf[len-1] << 8);

    if (ret != chk) {
	printf("Bad checksum: %d / calc: %d\n", ret, chk);
	return -1;
    }
    return 0;
}


int sendtoSQL(MYSQL *conn, char *query)
{
	if(conn == NULL) return -1;
	if (mysql_query(conn, query) != 0)
		mysql_print_error(conn);
		return 0;
}

char *OLD_RAIN;
void wmr_handle_rain(MYSQL *conn, WMR *wmr, unsigned char *data, int len)
{
    int sensor, power, rate;
    float hour, day, total;
    int smi, sho, sda, smo, syr;
    char *msg;
	char *zombie;
    
    sensor = data[2] & 0x0f;
    power = data[2] >> 4;
    rate = data[3];
    
    hour = ((data[5] << 8) + data[4]) * 25.4 / 100.0; /* mm */
    day = ((data[7] << 8) + data[6]) * 25.4 / 100.0; /* mm */
    total = ((data[9] << 8) + data[8]) * 25.4 / 100.0; /* mm */

    smi = data[10];
    sho = data[11];
    sda = data[12];
    smo = data[13];
    syr = data[14] + 2000;

 	asprintf(&msg, "INSERT RAIN %s VALUES (\"%d\", \"%d\", \"%d\" \"%.2f\" \"%.2f\" \"%.2f\"  \"%04d%02d%02d%02d%02d\");", TABLE_MAP_RAIN, sensor, power, rate, hour, day, total, syr, smo, sda, sho, smi);
	
	if(!strcmp(msg, OLD_RAIN))
	{
		    free(msg);
	}
	else
	{
		if(DEBUGIT) fprintf(stderr,"%s\n",msg);
		sendtoSQL(conn, msg);
		zombie = OLD_RAIN;
		OLD_RAIN = msg;
		free(zombie);
	}


}

char *const SMILIES[] = { "  ", ":D", ":(", ":|" };
char *const TRENDS[] = { "-", "U", "D" };

char *OLD_TEMP;

void wmr_handle_temp(MYSQL *conn, WMR *wmr, unsigned char *data, int len)
{
    int sensor, st, smiley, trend, humidity;
    float temp, dewpoint;
    char *smileyTxt = "";
    char *trendTxt = "";
    char *msg;
	char *zombie;

    sensor = data[2] & 0x0f;
    st = data[2] >> 4;
    smiley = st >> 2;
    trend = st & 0x03;

    if (smiley <= 3) smileyTxt = SMILIES[smiley];
    if (trend <= 2) trendTxt = TRENDS[trend];

    temp = (data[3] + ((data[4] & 0x0f) << 8)) / 10.0;
    if ((data[4] >> 4) == 0x8) temp = -temp;
    
    humidity = data[5];

    dewpoint = (data[6] + ((data[7] & 0x0f) << 8)) / 10.0;
    if ((data[7] >> 4) == 0x8) dewpoint = -dewpoint;
	
	asprintf(&msg, "INSERT TEMP %s VALUES (\"%d\", \"%d\", \"%s\", \"%.1f\", \"%d\", \"%.1f\" );", TABLE_MAP_TEMP, sensor, smiley, trendTxt, temp, humidity, dewpoint);
	
	if(!strcmp(msg, OLD_TEMP))
	{
		    free(msg);
	}
	else
	{
		if(DEBUGIT) fprintf(stderr,"%s\n",msg);
		sendtoSQL(conn, msg);
		zombie = OLD_TEMP;
		OLD_TEMP = msg;
		free(zombie);
	}


}

char *OLD_PRESSURE;
void wmr_handle_pressure(MYSQL *conn, WMR *wmr, unsigned char *data, int len)
{
    int pressure, forecast, alt_pressure, alt_forecast;
    char *msg;
	char *zombie;
	
    pressure = data[2] + ((data[3] & 0x0f) << 8);
    forecast = data[3] >> 4;
    alt_pressure = data[4] + ((data[5] & 0x0f) << 8);
    alt_forecast = data[5] >> 4;


	asprintf(&msg, "INSERT PRESSURE %s VALUES (\"%d\", \"%d\", \"%d\", \"d\");", TABLE_MAP_PRESSURE, pressure, forecast, alt_pressure, alt_forecast);
	
	if(!strcmp(msg, OLD_PRESSURE))
	{
		    free(msg);
	}
	else
	{
		if(DEBUGIT) fprintf(stderr,"%s\n",msg);
		sendtoSQL(conn, msg);
		zombie = OLD_PRESSURE;
		OLD_PRESSURE = msg;
		free(zombie);
	}

}

void wmr_handle_uv(MYSQL *conn, WMR *wmr, unsigned char *data, int len)
{
    char *msg;

	asprintf(&msg, "INSERT UV %s VALUES (\"UV\");", TABLE_MAP_UV);
	if(DEBUGIT) fprintf(stderr,"%s\n",msg);
	sendtoSQL(conn, msg);

    free(msg);
}

char *const WINDIES[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NWN" };

char *OLD_WIND;
void wmr_handle_wind(MYSQL *conn, WMR *wmr, unsigned char *data, int len)
{
    char *msg;
    int wind_dir, power, low_speed, high_speed;
    char *wind_str;
    float wind_speed, avg_speed;
	char *zombie;
	
    wind_dir = data[2] & 0xf;
    wind_str = WINDIES[wind_dir];
    power = data[2] >> 4;
    
    wind_speed = data[4] / 10.0;

    low_speed = data[5] >> 4;
    high_speed = data[6] << 4;
    avg_speed = (high_speed + low_speed) / 10.0;

   	asprintf(&msg, "INSERT WIND %s VALUES (\"%d\", \"%s\", \"%.1f\", \"%.1f\" );", TABLE_MAP_WIND, power, wind_str, wind_speed, avg_speed);
	
	if(!strcmp(msg, OLD_WIND))
	{
		    free(msg);
	}
	else
	{
		if(DEBUGIT) fprintf(stderr,"%s\n",msg);
		sendtoSQL(conn, msg);
		zombie = OLD_WIND;
		OLD_WIND = msg;
		free(zombie);
	}
	
}

//Take Time in YEAR-Month-Day Hour:Minue
time_t getmydrift(int yr,int mo,int dy,int hr,int mi)

{
	struct tm tmC;
	struct tm *check_time;
	time_t our_clock, epochC;

	
	//Get Time from system Clock
	epochC = time(NULL);
	check_time = localtime(&epochC);
	
	tmC.tm_sec = 0;
	tmC.tm_min = mi;
	tmC.tm_hour = hr;
	tmC.tm_mday = dy;
	tmC.tm_mon = mo - 1;
	tmC.tm_year = yr - 1900;
	
	tmC.tm_isdst = check_time->tm_isdst; //get dst
		
	our_clock = mktime(&tmC);

	//Return Difference
	return (epochC > our_clock) ? (epochC - our_clock) : (our_clock - epochC);
}

/*
//Random Thoughts - All Clock data will be different (Right?) - Can be used to measure drift from PC Clock?
Soooo.... Do I need 100 entries with time data in my SQL DB? I would say no - but having a clock time from the CO weather station may be nice to track.
My solution is to check for drift with PC Time. Save the drift and watch for a change in drift or other values.
*/
char OLD_CLOCK[255];
time_t OLD_DRIFT;
void wmr_handle_clock(MYSQL *conn, WMR *wmr, unsigned char *data, int len)
{
    int power, powered, battery, rf, level, mi, hr, dy, mo, yr;
    char *msg;

    power = data[0] >> 4;
    powered = power >> 3;
    battery = (power & 0x4) >> 2;
    rf = (power & 0x2) >> 1;
    level = power & 0x1;
	time_t drift;
	char Tbuff[255];
	char Vbuff[255];
	
    mi = data[4];
    hr = data[5];
    dy = data[6];
    mo = data[7];
    yr = data[8] + 2000;
	

	snprintf(Vbuff,250,"\"%d\", \"%d\", \"%d\",\"%d\"", powered, battery, rf, level);
	drift = getmydrift(yr,mo,dy,hr,mi);
	if(DEBUGIT) fprintf(stderr,"Got Clock Data - Drift is %d Old Drift is %d- Other data %s\n",drift,OLD_DRIFT, Vbuff);
	if(strcmp(Vbuff, OLD_CLOCK) && (OLD_DRIFT != drift))
	{
		asprintf(&msg, "INSERT CLOCK %s VALUES (\"%04d-%02d-%02d %02d:%02d\", \"%d\", \"%d\", \"%d\",\"%d\" );", TABLE_MAP_CLOCK, yr, mo, dy, hr, mi, powered, battery, rf, level);
		if(DEBUGIT) fprintf(stderr,"%s\n",msg);
		sendtoSQL(conn, msg);
		free(msg);
		strncpy(Vbuff,OLD_CLOCK,250);
		OLD_DRIFT = drift;
	}


}

void wmr_handle_packet(MYSQL *conn, WMR *wmr, unsigned char *data, int len)
{
    //dump_packet(data, len);
    
    switch(data[1]) {
    case 0x41:
	wmr_handle_rain(conn, wmr, data, len);
	break;
    case 0x42:
	wmr_handle_temp(conn, wmr, data, len);
	break;
    case 0x46:
	wmr_handle_pressure(conn, wmr, data, len);
	break;
    case 0x47:
	wmr_handle_uv(conn, wmr, data, len);
	break;
    case 0x48:
	wmr_handle_wind(conn, wmr, data, len);
	break;
    case 0x60:
	wmr_handle_clock(conn, wmr, data, len);
	break;
    }    
}

void wmr_read_data(MYSQL *conn, WMR *wmr)
{
    int i, j, unk1, type, data_len;
    unsigned char *data;

    /* search for 0xff marker */
    i = wmr_read_byte(wmr);
    while(i != 0xff) {
	i = wmr_read_byte(wmr);
    }

    /* search for not 0xff */
    i = wmr_read_byte(wmr);
    while(i == 0xff) {
	i = wmr_read_byte(wmr);
    }
    unk1 = i;

    /* read data type */
    type = wmr_read_byte(wmr);

    /* read rest of data */
    data_len = 0;
    switch(type) {
    case 0x41:
	data_len = 17;
	break;
    case 0x42:
	data_len = 12;
	break;
    case 0x46:
	data_len = 8;
	break;
    case 0x47:
	data_len = 5;
	break;
    case 0x48:
	data_len = 11;
	break;
    case 0x60:
	data_len = 12;
	break;
    default:
	printf("Unknown packet type: %02x, skipping\n", type);
    }

    if (data_len > 0) {
	data = malloc(data_len);
	data[0] = unk1;
	data[1] = type;
	for (j = 2; j < data_len; ++j) {
	    data[j] = wmr_read_byte(wmr);
	}

	if (verify_checksum(data, data_len) == 0) {
	    wmr_handle_packet(conn, wmr, data, data_len);
	}

	free(data);
    }

    /* send ack */
    wmr_send_packet_ready(wmr);
}





int startlog(MYSQL *conn)
{
	int ret;
	ssize_t bytesret;
	char onebyte;
	char *outbuffer = malloc(sizeof(char) * BSIZE+30);
	char *buffer = malloc(sizeof(char) * BSIZE+1);


	memset(buffer,'\0',BSIZE);
			
	sigset_t myset;
	sigfillset(&myset);
	sigdelset(&myset, SIGTERM);
	sigprocmask(SIG_BLOCK, &myset,NULL);
	
	if(signal(SIGTERM,sig_catcher) == SIG_ERR) { fprintf(stderr,"Error Setting up Signal Catcher!\n"); exit(2);}
	
	wmr = wmr_new();
	if (wmr == NULL) {
	fprintf(stderr, "wmr_new failed\n");
	exit(-1);
	}
	printf("Opening WMR100...\n");
	ret = wmr_init(wmr);
	if (ret != 0) {
	fprintf(stderr, "Failed to init USB device, exiting.\n");
	exit(-1);
	}

	printf("Found on USB: %s\n", wmr->hid->id);
	wmr_print_state(wmr);
	if( (sigsetjmp(jmpbuffer,1)) == 0 )
	{
		while(1)
		{
			wmr_read_data(conn, wmr);
		}
	}
	else
	{
		if(DEBUGIT==1) fprintf(stderr,"SIG TERM Called. Cleaning up.\n");
		wmr_close(wmr);
		printf("Closed WMR100\n");
		free(outbuffer);
		free(buffer);	
	}
	
	
}


void initTables(MYSQL *conn)
{
	if(conn == NULL) exit(1);
	
	char sqlbuffer[BIGBUFFER];
	
	fprintf(stderr,"Intializing Tables in Database\n");
	
	fprintf(stderr,"\tDropping table %s\n","TEMP");
	snprintf(sqlbuffer, BIGBUFFER,"DROP table %s;","TEMP");
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tCreating table %s\n","TEMP");
	snprintf(sqlbuffer,BIGBUFFER,"%s",TABLE_TEMP);
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tDropping table %s\n","RAIN");
	snprintf(sqlbuffer, BIGBUFFER,"DROP table %s;","RAIN");
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tCreating table %s\n","RAIN");
	snprintf(sqlbuffer,BIGBUFFER,"%s",TABLE_RAIN);
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tDropping table %s\n","PRESSURE");
	snprintf(sqlbuffer, BIGBUFFER,"DROP table %s;","PRESSURE");
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tCreating table %s\n","PRESSURE");
	snprintf(sqlbuffer,BIGBUFFER,"%s",TABLE_PRESSURE);
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);		
		
	fprintf(stderr,"\tDropping table %s\n","UV");
	snprintf(sqlbuffer, BIGBUFFER,"DROP table %s;","UV");
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tCreating table %s\n","UV");
	snprintf(sqlbuffer,BIGBUFFER,"%s",TABLE_UV);
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);

	fprintf(stderr,"\tDropping table %s\n","WIND");
	snprintf(sqlbuffer, BIGBUFFER,"DROP table %s;","WIND");
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tCreating table %s\n","WIND");
	snprintf(sqlbuffer,BIGBUFFER,"%s",TABLE_WIND);
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);		

	fprintf(stderr,"\tDropping table %s\n","CLOCK");
	snprintf(sqlbuffer, BIGBUFFER,"DROP table %s;","CLOCK");
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);
		
	fprintf(stderr,"\tCreating table %s\n","CLOCK");
	snprintf(sqlbuffer,BIGBUFFER,"%s",TABLE_CLOCK);
	if (mysql_query(conn,sqlbuffer) != 0)
		mysql_print_error(conn);

}



void printhelp()
{
	printf("WMR MySQL Data logger %s\n\n",VERSION_STRING);
	printf("\t-I, --intialize\t\t Intialize new tables (Warning: wipes data/drops old  tables).\n");
	printf("\t-U, --userid\t\t Run myheyu as another user. EX: -U nobody\n");
	printf("\t-d, --debug\t\t Debug Mode\n\n");
	printf("\tMySQL Options\n");
	printf("\t-H, --host=name\t\t Connect to MySQL Host Name\n");
    printf("\t-u, --user=name\t\t Connect to MySQL using User Name\n");
    printf("\t-p, --password=password\t Connect to MYSQL using password\n");
    printf("\t-P, --port=[#]\t\t Connect to MySQL using port number # (other then default)\n");
    printf("\t-s, --socket=file\t Connect to MYSQL using a UNIX Socket\n");

}

static const char *groups[] = {"client", 0};

struct option long_options[] = 
{
  {"host", required_argument, NULL, 'H'},
  {"user", required_argument, NULL, 'u'},
  {"password", required_argument, NULL, 'p'},
  {"port", required_argument, NULL, 'P'},
  {"socket", required_argument, NULL, 's'},
  {"help", no_argument, NULL, 'h'},
  {"debug", no_argument, NULL, 'd'},
  {"intialize", no_argument, NULL, 'I'},
  {"end", no_argument, NULL, 'E'},
  {"time", required_argument, NULL, 'T'},
  {"userid", required_argument, NULL, 'U'},
  { 0, 0, 0, 0 }
}; //long options



int main(int argc, char *argv[])
{
	char *host_name = NULL;
	char *user_name = NULL;
	char *password = NULL;
	char passbuffer[PASS_SIZE]; // Just in case we want a NULL Password parm - create a buffer to point to
	unsigned int port_num = 0;
	char *socket_name = NULL;
	MYSQL *conn = NULL;
    int input,option_index,key;

	unsigned short INIT_FLAG = 0;
	unsigned short FILE_FLAG = 0;
	char fullname[PATH_MAX];
	struct passwd *passwdptr;
	char username[255];
	asprintf(&OLD_TEMP,"EMPTY");
	asprintf(&OLD_RAIN,"EMPTY");
	asprintf(&OLD_PRESSURE,"EMPTY");
	asprintf(&OLD_WIND,"EMPTY");
	snprintf(OLD_CLOCK,255,"EMPTY");
	
	DEBUGIT = 0;
	pid_t pid;


   //Setup for MySQL Init File
    my_init();
    
    //Load Defaults -- adds file contents to arugment list - Thanks MySql
    load_defaults("myheyu", groups, &argc, &argv);
    int ret;
   while((input = getopt_long(argc, argv, "H:u:p:P:s:dhIU:", long_options, &option_index)) != EOF )
    {
      switch(input)
      {
			case 'd':	
				DEBUGIT = 1;
			break;
			
			case 'h' :
				printhelp();
				return (0);
			break;
			
			case 'H' :
				host_name = optarg;
			break;
			
			case 'u' :
				user_name = optarg;
			break;	
			
			case 'p' :
				snprintf(passbuffer,PASS_SIZE,"%s\0",optarg);
				password = passbuffer; // no more NULL
				//Neat trick from MySQL C API Book - Online to hide password from PS
				while (*optarg) *optarg++ = ' ';
			break;	
			
			case 'P' :
				port_num = (unsigned int) atoi(optarg);
			break;
			case 's' :
				socket_name = optarg;  
			break;
			
			case 'I' :
				INIT_FLAG = 1;
			break;


			case 'U' :
				//Change User
				//Find User ID in password
				strncpy(username,optarg,250);
				passwdptr = getpwnam(username);
				if(passwdptr->pw_uid > 0)
				{
					printf("Setting proccess to User: %s User ID: %d\n",username, passwdptr->pw_uid);
					if (setuid(passwdptr->pw_uid) == -1)
						{fprintf(stderr,"Error changing user id. Can't SETUID\n"); exit(1);}
					
				}
				else {fprintf(stderr,"Error changing user id. Can't find user specified on system.\n"); exit(1);}
				
				
			break;
  
      }//switch      
     
    }//while
    
	//Do a User Check
	if(getuid() == 0)
		fprintf(stderr,"Warning: Running as Root.\n");
	else
	{
		if( (passwdptr = getpwuid(getuid())) == NULL)
			{fprintf(stderr,"Warning: Can't find the running user ID.\n");}
		else
		{
			fprintf(stderr,"Running as User: %s User ID: %d\n",passwdptr->pw_name, passwdptr->pw_uid);
		}
	}
	
	
	//Setup SQL  
	conn = mysql_connect(host_name,user_name,password,db_name, port_num, socket_name,0);

	if(INIT_FLAG == 1)
	{
		initTables(conn);	
		if (mysql_query(conn,"COMMIT;") != 0)
			mysql_print_error(conn);
		mysql_close(conn);
		
	}
	else if(DEBUGIT == 0) // FORK
	{
		fprintf(stderr,"Forking this process now.\n");
		if( (pid = fork()) < 0) {fprintf(stderr,"Error Forking!\n"); exit(-1); }
		else if (pid == 0) // Run child as Dameon
		{ 
			startlog(conn);
		}
		else // Parent Exit Now
		{
			return(0);
		}
	}
	else //Run in Foreground
	{
		fprintf(stderr,"We did not fork. We are in Debug mode.\n");
		startlog(conn);
	}
   
	return 0;

}
