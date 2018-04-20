

#include "myunp.h"
#include <inttypes.h> /* strtoumax */
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#define ARRAY_SIZE_MAX 1000
#define MAX_SERVERS 20
#define DELIM "|"
#define PENDING 0
#define RUNNING 1
#define DONE 2
#define AVAILABLE 3
#define FAILED 4
#define DOWN 5
#define NO_SERVERS_FUNCTIONING -10
#define ALL_SERVERS_BUSY -20
#define MAX_CHUNKS 20

struct thread_args
{
	int chunk_number; 
	int chunk_size;
	int server_index;
	char * file_name;
};


struct server
{
	char ip_addr[200];
	char port_number[20];
	int status;
	int chunk_number;
	char *data;
	pthread_t tid;
	struct thread_args t_args;
} servers[MAX_SERVERS];

int server_count = 0;

struct chunk
{
	int number;
	int status;
	int server_index;
	char *data;
} chunks[MAX_CHUNKS];

int chunk_count = 0;

int get_line(FILE *, char*);
int get_servers(char *, struct server[], int *);
int add_server(char *, struct server[], int *);
void print_servers(struct server[], int);
int parse_server_filesize_header(char *, int *, int *, char *);
void * get_chunk_from_server(void *);
int define_chunks(int, int);

int main(int argc, char **argv)
{
	//initializations
	int num_connections;
	char command[MAXLINE];
	int     sockfd, n, i, j, k, start, bytes_read, chunk_size, chunk_number;
	char port_number[30];
	char    recvline[MAXLINE + 1];
	struct sockaddr_in servaddr;
	char *c;
	char    buff[MAXLINE];
	char client_header[ARRAY_SIZE_MAX];
	char    result_receive[ARRAY_SIZE_MAX];
	char work_line[ARRAY_SIZE_MAX];
	char header_from_server[2];
	char ip_addr_of_server[30];
	char line[ARRAY_SIZE_MAX];
	char *token;
	char error_msg[ARRAY_SIZE_MAX];
	char filename[100];
	char chunk_header_from_server[ARRAY_SIZE_MAX];
	int line_size;
	int return_code;
	int file_size;
	bool ip_found;
	FILE *fp;
	bool nl_found, end_loop, uneven;
	
	//check for valid input: user must specify ip address and port number
	if (argc != 3)
		err_quit("usage: myclient <server-file.text> <num-connections>");
		
	num_connections = atoi(argv[2]);
	
	//add servers
	get_servers(argv[1], servers, &server_count);

	sockfd = connect_to_server();
	if(sockfd == 0) {
		err_quit("cannot connect to any server\n");
	}
	
	//request file size from server	
	get_file_name(filename);
	make_filename_header(client_header, filename);
	Write(sockfd, client_header, strlen(client_header));
	
	//Receive file size header from server
	Read(sockfd, header_from_server, ARRAY_SIZE_MAX);
	parse_server_filesize_header(header_from_server, &return_code, &file_size, &error_msg[0]);
	
	//close connection with server
	Close(sockfd);
	
	if(return_code == 0) {
	
		//divide up string into chunks
		chunk_count = define_chunks(server_count, num_connections);
		chunk_size = file_size / chunk_count;
		if(file_size % chunk_count != 0) {
			chunk_size++;
		}
		process_chunks(chunk_size, server_count, filename);

		exit(0);
	} else {
	
		err_quit(error_msg);
	}
}

int connect_to_server() {
	char ip_addr_of_server[100];
	char *c;
	struct sockaddr_in servaddr;
	int i, sockfd;
	
	//create socket
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;
	
	for(i = 0; i < server_count; i++) {
		strcpy(ip_addr_of_server, servers[i].ip_addr);
   	intmax_t port = strtoimax(servers[i].port_number, &c, 10);

		//initialize socket
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons((uint16_t) port);
		if (inet_pton(AF_INET, ip_addr_of_server, &servaddr.sin_addr) <= 0) {
			servers[i].status = DOWN;
			continue;
		}

		//connect to server
		if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
			servers[i].status = DOWN;
			continue;
		}
		return sockfd;
	}
	return 0;
}

int get_file_name(char * file_name) {

	//scan user input for name of file
	printf("Please enter name of file to be transferred\n");
	scanf("%s", file_name);
}

int get_line(FILE *fp, char* line) {

	//extract single line from server file
	int j;
	j = 0;
	do {
      *(line + j) = fgetc(fp);
      if( feof(fp) ) {
         break ;
      }
      j++;
   } while(1);
   *(line + j) = 0;
   return j;
}

int get_servers(char *file_name, struct server servers[], int *num_servers) {

	//extract ip addresses and port numbers from server file
	FILE *fp = fopen(file_name,"r");
	if(fp == NULL) {
		err_quit("bad file\n");
	}
	char ch;
	int j = 0;
	char line[ARRAY_SIZE_MAX];
	do {
		ch = fgetc(fp);
		if(feof(fp)) {
			line[j] = 0;
			if(j > 0) {
				add_server(line, servers, num_servers);
			}
			break;
		}
		else if(ch == '\n') {
			line[j] = 0;
			add_server(line, servers, num_servers);
			j = 0;
		}
		else {
			line[j] = ch;
			j++;
		}
	} while(1);
}

int add_server(char * line, struct server servers[], int * num_servers) {

	//fill server struct based on info in server file
	char ip_addr[200];
	char port_number[20];
	strcpy(ip_addr, strtok(line, " "));
	strcpy(port_number, strtok(NULL, " "));
	strcpy(servers[*num_servers].port_number, port_number);
	strcpy(servers[*num_servers].ip_addr, ip_addr);
	servers[*num_servers].status = AVAILABLE;
	servers[*num_servers].chunk_number = -1;
	servers[*num_servers].data = NULL;
	(*num_servers)++;
}

void print_servers(struct server servers[], int num_servers) {

	//debugging function for printing out list of servers and their attributes
	int i;
	for (i = 0; i < num_servers; i++) {
		printf("%s %s\n", servers[i].ip_addr, servers[i].port_number);
	}
}

int make_filename_header(char * header, char * filename) {

	//create file name header to send to server
	strcpy(header, "FILE");
	strcat(header, DELIM);
	strcat(header, filename);
}

int make_client_chunk_header(char * header, int chunk_size, int chunk_number, char * file_name) {

	//create chunk header to send to server
	char temp[100];
	strcpy(header, "CHUNKNUMBER");
	strcat(header, DELIM);
	strcat(header, file_name);
	strcat(header, DELIM);
	sprintf(temp, "%d", chunk_number);
	strcat(header, temp);
	strcat(header, DELIM);
	strcat(header, "SIZE");
	strcat(header, DELIM);
	sprintf(temp, "%d", chunk_size);
	strcat(header, temp);
}

int parse_server_filesize_header(char * header, int * return_code, int * file_size, char * error_msg) {

	//extract return code file size and possible error message from server's header
	char temp[100];
	char junk[ARRAY_SIZE_MAX];
	strcpy(junk, strtok(header, DELIM));
	strcpy(temp, strtok(NULL, DELIM));
	*return_code = atoi(temp);
	strcpy(temp, strtok(NULL, DELIM));
	if(*return_code == 0) {
		*file_size = atoi(temp);
	} else {
		strcpy(error_msg, temp);
	}
}

int define_chunks(int num_servers, int num_connections) {

	//fill chunk struct
	int chunk_count, i;
	if(num_servers < num_connections)
		chunk_count = num_servers;
	else
		chunk_count = num_connections;
	for (i = 0; i < chunk_count; i++) {
		chunks[i].number = i;
		chunks[i].status = PENDING;
		chunks[i].server_index = -1;
		chunks[i].data = NULL;
	}
	return chunk_count;
}

int process_chunks(int chunk_size, int num_servers, char * file_name) {

	//loop through chunk count
	int i, j, server_index;
	int chunk_countdown = chunk_count;
	while(1) {
	
		//request chunks from servers using separate threads
		for(i = 0; i < chunk_count; i++) {
			if(chunks[i].status == PENDING) {
				server_index = create_server_thread(i, chunk_size, file_name);
				if(server_index == NO_SERVERS_FUNCTIONING) {
					err_quit("No servers are functioning\n");
				}
				
				chunks[i].server_index = server_index;
				if(chunks[i].server_index != ALL_SERVERS_BUSY) {
					chunks[i].status = RUNNING;
				}
			}	
		}
		
		//check server threads
		for(i = 0; i < chunk_count; i++) {
			if(chunks[i].status == RUNNING) {
				void *status = 0;
				int j = chunks[i].server_index;
				pthread_join(servers[j].tid, &status);
				if(servers[j].status == DONE) {
					chunks[i].status = DONE;
					chunks[i].server_index = -1;
					chunks[i].data = servers[j].data;
					servers[j].data = NULL;
					servers[j].status = AVAILABLE;
					servers[j].chunk_number = -1;
					chunk_countdown--;
				} 
				if(servers[j].status == FAILED) {
					chunks[i].status = PENDING;
					chunks[i].server_index = -1;
					servers[j].status = DOWN;
					servers[j].chunk_number = -1;
				}
			}
		}
		
		//sleep if chunks are in process else exit loop
		if(chunk_countdown > 0) {
			sleep(1);
		} else {
			break;
		}
	}
	
	//write out chunks
	char out_file_name[100];
	strcpy(out_file_name, file_name);
	strcat(out_file_name, ".out");
	FILE *fp = fopen(out_file_name,"w");
	if(fp == NULL) {
		err_quit("can't write file\n");
	}
	for(j = 0; j < chunk_count; j++) {
		for(i = 0; i < strlen(chunks[j].data); i++) {
			fputc(chunks[j].data[i], fp);
		}
		free(chunks[j].data);
	}
	fclose(fp);
	printf("data written to file: %s\n", out_file_name);
}	

int create_server_thread(int chunk_number, int chunk_size, char * file_name) {

	//create appropriate number of threads
	bool no_servers_functioning = true;
	int i, return_code;
	for(i = 0; i < server_count; i++) {
		if(servers[i].status == AVAILABLE) {
			servers[i].t_args.chunk_number = chunk_number;
			servers[i].t_args.chunk_size = chunk_size;
			servers[i].t_args.server_index = i;
			servers[i].t_args.file_name = file_name;
			return_code = pthread_create( &servers[i].tid, NULL, get_chunk_from_server, (void*) &servers[i].t_args);
			if(return_code != 0) {
				servers[i].status = FAILED;
			} else {
				servers[i].status = RUNNING;
				servers[i].chunk_number = chunk_number;
				return i;
			}
		}
		no_servers_functioning = (no_servers_functioning && (servers[i].status == DOWN));
	}
	if(no_servers_functioning)
		return NO_SERVERS_FUNCTIONING;
	else
		return ALL_SERVERS_BUSY;
}

void * get_chunk_from_server(void * ptr) {

	struct thread_args *t_args = (struct thread_args *) ptr;
	int chunk_number = t_args->chunk_number;
	int chunk_size = t_args->chunk_size; 
	int server_index = t_args->server_index; 
	char * file_name = t_args->file_name;
	int sock;
	char *c;
	char chunk_header_from_server[ARRAY_SIZE_MAX];
	struct sockaddr_in server_address;
	char client_header[ARRAY_SIZE_MAX];
	
	if ( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		servers[server_index].status = FAILED;
		return;
	}
		
	//initialize socket
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	intmax_t port = strtoimax(servers[server_index].port_number, &c, 10);
	server_address.sin_port = htons((uint16_t) port);
	if (inet_pton(AF_INET, servers[server_index].ip_addr, &server_address.sin_addr) <= 0) {
		servers[server_index].status = FAILED;
		
		return;
	}

	//connect to server
	if (connect(sock, (SA *) &server_address, sizeof(server_address)) < 0) {
		servers[server_index].status = FAILED;
		
		return;
	}
	
	//send header containing size of chunk
	make_client_chunk_header(client_header, chunk_size, chunk_number, file_name);
	Write(sock, client_header, strlen(client_header));
	
	//Get chunk from server
	Read(sock, chunk_header_from_server, 12);
	servers[server_index].data = (char*)malloc(chunk_size);
	int n = 0;
	int t = 0;
	while(1) {
		n = Read(sock, servers[server_index].data + t, chunk_size);
		t += n;
		if(n == 0)
			break;
	}

	servers[server_index].status = DONE;
	return;
}