
#define TABLE_TEMP "create table TEMP ( \
id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, \
timestamp TIMESTAMP DEFAULT NOW(), \
sensor TINYINT UNSIGNED ZEROFILL, \
smile TINYINT UNSIGNED ZEROFILL, \
trend  char(1), \
temp float ZEROFILL, \
humidity TINYINT UNSIGNED ZEROFILL, \
dewpoint float ZEROFILL \
);"

#define TABLE_MAP_TEMP "(sensor, smile, trend, temp, humidity, dewpoint)"

#define TABLE_RAIN "create table RAIN ( \
id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, \
timestamp TIMESTAMP DEFAULT NOW(), \
sensor TINYINT UNSIGNED ZEROFILL, \
power TINYINT UNSIGNED ZEROFILL, \
rate  TINYINT UNSIGNED ZEROFILL, \
hour_total float ZEROFILL, \
day_total float ZEROFILL, \
all_total float ZEROFILL, \
since VARCHAR(20) \
);"

#define TABLE_MAP_RAIN "(sensor, power, rate, hour_total, day_total, all_total, since)"

#define TABLE_PRESSURE "create table PRESSURE ( \
id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, \
timestamp TIMESTAMP DEFAULT NOW(), \
pressure TINYINT UNSIGNED ZEROFILL, \
forecast TINYINT UNSIGNED ZEROFILL, \
altpressure  TINYINT UNSIGNED ZEROFILL, \
altforecast TINYINT UNSIGNED ZEROFILL \
);"

#define TABLE_MAP_PRESSURE "(pressure, forecast, altpressure, altforecast)"

#define TABLE_UV "create table UV ( \
id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, \
timestamp TIMESTAMP DEFAULT NOW(), \
type VARCHAR(10) \
);"

#define TABLE_MAP_UV "(type)"

#define TABLE_WIND "create table WIND( \
id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, \
timestamp TIMESTAMP DEFAULT NOW(), \
power TINYINT UNSIGNED ZEROFILL, \
dir VARCHAR(5), \
speed TINYINT UNSIGNED ZEROFILL, \
avgspeed  TINYINT UNSIGNED ZEROFILL \
);"

#define TABLE_MAP_WIND "(power, dir, speed, avgspeed)"

#define TABLE_CLOCK "create table CLOCK ( \
id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, \
timestamp TIMESTAMP DEFAULT NOW(), \
clock VARCHAR(20), \
powered TINYINT UNSIGNED ZEROFILL, \
battery TINYINT UNSIGNED ZEROFILL, \
rf TINYINT ZEROFILL, \
level TINYINT ZEROFILL \
);"

#define TABLE_MAP_CLOCK "(clock, powered, battery, rf, level)"



#define BIGBUFFER 10000


/*
INPUT: MySQL Handler

Outputs to STDERR - Errno and message
*/
void mysql_print_error(MYSQL *);

/*
INPUT
hostname 
username
password
database name
port number
socket name

NULL == Default

Returns pointer to MySQL handler or it exits the program
*/
MYSQL *mysql_connect(char *, char *, char *, char *, unsigned int, char *,unsigned int);
