
#include "unp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <time.h>


#define ARRAY_SIZE_MAX 1000
#define DELIM "/"

int fsize(FILE*);
int parse_client_filename_header(char *, char *);
int make_server_file_size_header(int, int, char *);
int parse_client_chunk_header(char *, int *, int *, char *);
int make_server_chunk_header(char *, int, int);
struct hostent dns_lookup(char *);
void write_log(char *, char *, char *, char *, char *, char *, char *);
void child_process(int arc, char **arv);
void make_lower(char *);



struct  sockaddr_in servaddr;
int     listenfd, connfd, sitefd, n;
char client_request[ARRAY_SIZE_MAX], web_name[ARRAY_SIZE_MAX], * site_reply, bad_req_reply[ARRAY_SIZE_MAX],
	req_type[ARRAY_SIZE_MAX], http_vers[ARRAY_SIZE_MAX], uri[ARRAY_SIZE_MAX], server_address[ARRAY_SIZE_MAX],
	action[ARRAY_SIZE_MAX], out_file_name[ARRAY_SIZE_MAX];
bool fin, err_flag, file_closed;
char html_msg[ARRAY_SIZE_MAX];
pid_t pid;


int main(int argc, char **argv)
{

	
	strcpy(html_msg, "<html><title>403 Forbidden URL</title><body>403 Forbidden URL</body></html>");
	strcpy(req_type, "not retrieved");
	strcpy(server_address, "not retrieved");
	strcpy(web_name, "not retrieved");
		
	//check for command line usage error
	if(argc != 3)
		err_quit("usage: myserver <port number> <forbidden-sites>");

	//initialize socket
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1])); /* daytime server */
	
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	//bind socket
	Bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
	
	//set to listening
	Listen(listenfd, LISTENQ);
	
	//prepare log
	strcpy(out_file_name, "log");                
	
	// listening loop
	while(1) {
	
		//allocate memory for buffers
		site_reply = (char *) malloc(ARRAY_SIZE_MAX);

		//accept connection from client
		connfd = Accept(listenfd, (SA *) NULL, NULL);
		
		//fork child process
		if((pid = fork()) < 0) {
			err_quit("fork error");
		} else if(pid > 0) {
			
			//close connection with client
			Close(connfd);
			free(site_reply);
		} else {
			child_process(argc, argv);
		}
	}
}	


void child_process(int arc, char **arv) {
	Close(listenfd);

	bzero(&client_request, sizeof(client_request));

	//read client request
	while(1) {
		n = read(connfd, client_request, ARRAY_SIZE_MAX);	
		if(n < ARRAY_SIZE_MAX)
			break;
	}
	
	extract_web_name(client_request, &web_name);

	//initialize action error flag and error type
	strcpy(action, "none");
	strcpy(bad_req_reply, "none");
	err_flag = false;

	//get http version from header
	get_version(client_request, &http_vers);
	
	//check for bad site
	if(check_site(arv[2], web_name) == 0) {
		strcpy(bad_req_reply, "403 Forbidden URL");
		printf("forbidden site detected\n");
		Write(connfd, html_msg, strlen(html_msg));
		strcpy(action, "filtered");
		err_flag = true;
	
		write_log(req_type, http_vers, web_name, server_address, action, bad_req_reply, out_file_name);
		
		//close connection with client and free variables
		Close(connfd);
		free(site_reply);
		exit(0);
	}
	
	
	//check for bad client header
	if(check_request(client_request, &req_type) == 0) {
		strcpy(bad_req_reply, "405 Method not allowed");
		printf("bad request '%s'detected\n", req_type);
		Write(connfd, bad_req_reply, strlen(bad_req_reply));
		strcpy(action, "dropped");
		err_flag = true;
	
		write_log(req_type, http_vers, web_name, server_address, action, bad_req_reply, out_file_name);

		//close connection with client and free variables
		Close(connfd);
		free(site_reply);
		exit(0);
	}

	//connect to requested site
	sitefd = connect_to_site(web_name, &server_address);
	if(sitefd == 0) {
		err_quit("cannot connect to site\n");
	}

	//forward client request to site
	Write(sitefd, client_request, strlen(client_request));

	fin = false;
	while(1) {

		//receive reply from site
		n = read(sitefd, site_reply, ARRAY_SIZE_MAX);
		if(n < ARRAY_SIZE_MAX)
			fin = true;

		//forward site reply to client
		Write(connfd, site_reply, n);
		if(fin)
			break;
	}

	//update action
	if(!err_flag)
		strcpy(action, "forwarded");

	//write to log
	write_log(req_type, http_vers, web_name, server_address, action, bad_req_reply, out_file_name);
	
	Close(connfd);
	exit(0);
}
	
	

int get_version(char * request, char * http_vers) {
	char temp[ARRAY_SIZE_MAX], *token, line[ARRAY_SIZE_MAX];
	int i;
	strcpy(temp, request);
	i = 0;
	while(1) {
		line[i] = temp[i];
		if(line[i] == '\n') {
			line[i] = '\0';
			break;
		}
		i++;
	}
	strcpy(http_vers, line + strlen(line) - 4);
	http_vers[strlen(http_vers) - 1] = '\0';
	return 0;
}

int connect_to_site(char * web_name, char * server_address) {
	int sitefd;
	char ip_addr[100], **pptr, str[ARRAY_SIZE_MAX];
	struct hostent *host;
	struct sockaddr_in servaddr;
	
	host = malloc(sizeof(struct hostent));
	
	
	//create socket
	if ( (sitefd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;
		
	sleep(2);
	
	//set port to well known port number
	intmax_t port = strtoimax("80");
	
	//initialize socket
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons((uint16_t) port);
	
	//perform DNS lookup of client specified name
	*host = dns_lookup(web_name);
	pptr = host->h_addr_list;
	
	//loop through list to find valid ip address
	for( ; *pptr != NULL; pptr++) {
		strcpy(ip_addr, inet_ntop(host->h_addrtype, *pptr, str, sizeof(str)));
		
		//set ip address
		if (inet_pton(AF_INET, ip_addr, &servaddr.sin_addr) <= 0) 
			continue;
			
		//connect to site
		if (connect(sitefd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
			continue;
		}	
		strcpy(server_address, ip_addr);
		free(host);	
		return sitefd;
	}
	return 0;	
}

struct hostent dns_lookup(char * web_name) {
	char **pptr, str[ARRAY_SIZE_MAX];
	struct hostent *host;
	
	//perform dns lookup
	if((host = gethostbyname(web_name)) == NULL) {
		printf("gethostbyname error for host: %s\n", web_name);
		exit(1);
	}
	
	return *host;
}

int extract_web_name(char * request, char * web_name) {
	char temp[ARRAY_SIZE_MAX], *token;
	char *p;
	strcpy(temp, request);
	make_lower(temp);
	token = strtok(temp, " ");
	token = strtok(NULL, " ");
	if(strstr(token, "http://") == NULL) {
		if(token[strlen(token)-1] == '/')
			token[strlen(token)-1] = '\0';
		strcpy(web_name, token);	}	
	else {
		if(token[strlen(token)-1] == '/')
			token[strlen(token)-1] = '\0';
		strcpy(web_name, token + 7);
	}
	p = strstr(web_name, ":");
	if(p != NULL) {
		*p = '\0';
	}
	return 0;
}

void make_lower(char * str) {
	int i;
	for(i = 0; str[i]; i++){
  		str[i] = tolower(str[i]);
  	}
}

int check_request(char * request, char * req_type) {
	
	//copy request type into string
	char temp[ARRAY_SIZE_MAX];
	strcpy(temp, request);	
	strcpy(req_type, strtok(temp, " "));
	
	//check if request type is valid
	if(strcmp(req_type, "GET") == 0)
		return 1;
		
	if(strcmp(req_type, "HEAD") == 0)
		return 1;
		
	return 0;
}

int check_site(char * file_name, char * web_name) {
	char line[ARRAY_SIZE_MAX];
	if(check_line(file_name, web_name) == 0)
			return 0;
		
	return 1;

}

int check_line(char * file_name, char * web_name) {

	//check file for bad sites
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
				if(strcmp(web_name, line) == 0) {
					fclose(fp);
					return 0;
				}
			}
			fclose(fp);
			break;
		}
		else if(ch == '\n') {
			line[j] = 0;
			if(strcmp(web_name, line) == 0) {
				fclose(fp);
				return 0;
			}
			j = 0;
		}
		else {
			line[j] = ch;
			j++;
		}
	} while(1);
	return 1;
}
	
void write_log(char * req_type, char * http_vers, char * web_name, char * server_address, char * action, char * bad_req_reply,
	char * out_file_name) {
	char str[ARRAY_SIZE_MAX * 10], temp[2], date[ARRAY_SIZE_MAX];
	time_t ticks;
	ticks = time(NULL);
	FILE *fp = fopen(out_file_name,"a");
	if(fp == NULL) {
		err_quit("can't write file\n");
	}
	
	sprintf(date, "%s", ctime(&ticks));
	date[strlen(date)] = '\0';
	strcat(str, date);
	strcat(str, "Request type: ");
	strcat(str, req_type);
	strcat(str, ", ");
	strcat(str, "HTTP version used: ");
	strcat(str, http_vers);
	strcat(str, ", ");
	strcat(str, "Requesting host name: ");
	strcat(str, web_name);
	strcat(str, ", ");
	strcat(str, "URI: ");
	strcat(str, server_address);
	strcat(str, ", ");
	strcat(str, "Server address: ");
	strcat(str, server_address);
	strcat(str, ", ");
	strcat(str, "Action taken: ");
	strcat(str, action);
	strcat(str, ", ");
	strcat(str, "Errors: ");
	strcat(str, bad_req_reply);
	strcat(str, "\n");
	str[strlen(str)] = '\0';
	
	fputs(str, fp);
	
	fclose(fp);
}
	
int check_valid_header(char * header) {
	if((strstr(header, "HTTP/1.1") == NULL) && (strstr(header, "HTTP/1.0") == NULL))
		return 0;
	if(strstr(header, "HTTP:") == NULL)
		return 0;
	return 1;
}
	