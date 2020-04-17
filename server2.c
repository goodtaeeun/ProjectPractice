// Partly taken from https://www.geeksforgeeks.org/socket-programming-cc/

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <wiringPi.h>
#include <time.h>
#include <stdint.h>


#define CDS 4 // light pin num
#define DHTPIN 29 //temp pin num

int check_cup() {
	//wiringPiSetup();
	pinMode(CDS, INPUT);

	if(digitalRead(CDS) == HIGH) {
		return 0;
	}
	else{
		return 1;
	}
//	return 0;
}

float
check_temp ()
{
	int d[5] = {0, 0, 0, 0, 0} ;
	uint8_t i, j = 0;
	uint8_t counter = 0;

	//wiringPiSetup();
	pinMode(DHTPIN, OUTPUT);
	digitalWrite(DHTPIN, LOW);
	delay(18) ;

	digitalWrite(DHTPIN, HIGH);
	uint8_t lastState = HIGH;

	delayMicroseconds(20);

	pinMode(DHTPIN, INPUT);

	for(i = 0; i < 85 ; i++) {
		counter = 0;
		while(digitalRead(DHTPIN) == lastState) {
			counter++;
			delayMicroseconds(1);
			if (counter == 255)
				break;
		}
		lastState = digitalRead(DHTPIN);

		if (counter == 255) {
			printf(".");
			break;
		}
		if(i >= 4 && i % 2 == 0) {
			d[j/8] <<= 1;
			if (counter > 30)
				d[j/8] |= 1;
			j++;
		}
	}

	while(1){
	if (j >= 40 && (d[4] == ((d[0] + d[1] + d[2] + d[3]) & 0xFF))){
		char buf [50];
		sprintf(buf, "%d.%d",d[2], d[3]);
		
		float f;
		f = atof(buf);

		return f;
	}


	}
}

int depth(){

	int trig = 23;
	int echo = 24;
	int start, end;
	float distance;

	if(wiringPiSetup() == -1) exit(1);

	pinMode(trig,OUTPUT);
	pinMode(echo,INPUT);

	
		digitalWrite(trig,LOW);
		delay(500);
		digitalWrite(trig,HIGH);
		delayMicroseconds(10);
		digitalWrite(trig,LOW);
		while(digitalRead(echo)==0);
		start = micros();
		while(digitalRead(echo)==1);
		end = micros();
		distance = (end-start)/29./2.;
		

		return (int)distance;

	
}


void *
worker (void * arg)
{
	int conn ;
	
	char * orig = 0x0;
	char store[1024];
	char * data = store;
	int len = 0 ;
	int s ;

	conn = *((int *)arg) ; 
	free(arg) ;

	float max_temp = 0;
	float current_temp = 0;
	int const cup_depth = 11;
	int water_height;
	time_t begin_time;
	time_t current_time;
	float time_dif;

	char buf[50];
	char * buffer = buf;

	while(1) //biggest loop
	{
	//data = store

	//data = strdup("HTTP/1.1 200 OK\r\nContent-Type: text/html; carset=UTF-8\r\nContent-Lenght:19\r\n\r\n<html>");

	
	// receive cup check data
	wiringPiSetup();
	water_height = depth();
	if(water_height > 3){
	//set begin_time
	time(&begin_time);
		
	while(check_cup()) //cup check loop
	{

		data = store;
		data = strdup("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Lenght:19\r\n\r\n<html>");
		data = realloc(data,sizeof(char) *1024);//to prevent memory error
		strcat(data, "Your cup is on the device");

		//check current_temp
		//receive water_height
		//wiringPiSetup();
		 current_temp = check_temp();

		//if(max_temp < current_temp)
		//{
		//	max_temp = current_temp;
		//	strcat(data, "<br>Checking your cup temparature");
	//	}
	//	else
		{
			sprintf(buf,"<br>Your cup is %f degrees",current_temp);
			strcat(data,buf);
			sprintf(buf, "<br>Your cup has dropped %f degrees", max_temp - current_temp);
			strcat(data,buf);
		}
		time(&current_time);
		time_dif = difftime(begin_time,current_time);//check time differnece

		sprintf(buf,"<br>Your cup has been placed over %d minuite %d seconds ",(int)time_dif/60, (int)time_dif%60);
		
		strcat(data,buf);
		strcat(data, ctime(&begin_time));

		water_height = depth();
		if(water_height > 3){
			sprintf(buf,"<br>Your cup has %dcm of water", cup_depth - water_height);
			strcat(data,buf);
		}
		else{
			sprintf(buf, "<br>There is no cup");
		}
		strcat(data,"<html>\r\n\r\n");
		len = strlen(data);
		while(len > 0 && (s = send(conn, data, len, 0)) > 0) {
			data += s;
			len -= s;
			}
	//	free(data);
//		shutdown(conn, SHUT_WR);
		

	}
	
	}
	else{
		data = store;
		data = strdup("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Lenght:19\r\n\r\n<html>");
		
		data = realloc(data,sizeof(char)*100);
		strcat(data,"<br>There is no cup<html>\r\n\r\n");

		len = strlen(data);
		while(len > 0 && (s = send(conn, data, len, 0)) > 0) {
			data += s;
			len -= s;
		}
	//	free(data);

//		shutdown(conn, SHUT_WR);
		
	}

	

	orig = data;


	}
	shutdown(conn, SHUT_WR) ;
	if (orig != 0x0) 
		free(orig) ;

	return 0x0 ;
}

int 
main (int argc, char const *argv[]) 
{ 
	int listen_fd, new_socket ; 
	struct sockaddr_in address; 
	int opt = 1; 
	int addrlen = sizeof(address); 
	
	if (argc != 2) {
		fprintf(stderr, "Wrong number of arguments") ;
		exit(EXIT_FAILURE) ;
	}

	char buffer[1024] = {0}; 

	listen_fd = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0 /*IP*/) ;
	if (listen_fd == 0)  { 
		perror("socket failed : "); 
		exit(EXIT_FAILURE); 
	}
	
	memset(&address, '0', sizeof(address)); 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY /* the localhost*/ ; 
	address.sin_port = htons(atoi(argv[1])); 

	if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed : "); 
		exit(EXIT_FAILURE); 
	} 

	while (1) {
		if (listen(listen_fd, 16 /* the size of waiting queue*/) < 0) { 
			perror("listen failed : "); 
			exit(EXIT_FAILURE); 
		} 

		new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen) ;
		if (new_socket < 0) {
			perror("accept"); 
			exit(EXIT_FAILURE); 
		} 

		pthread_t worker_tid ;
		int * worker_arg = (int *) malloc(sizeof(int)) ;
		*worker_arg = new_socket ;

		pthread_create(&worker_tid, 0x0, worker, (void *) worker_arg) ;
	}
} 

