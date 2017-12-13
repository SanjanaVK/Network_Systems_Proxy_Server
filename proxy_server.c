#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netdb.h>
#include<errno.h>
#include<sys/sendfile.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stddef.h>
#include<openssl/md5.h>
#include<sys/signal.h>
#include<time.h>
#include<memory.h>
#include<dirent.h>
#include<semaphore.h>
#include <pthread.h>

#define MAXBUF 2048
static sem_t *mutex;
static int expire_time;

struct cache_s
{
	char md5sum_cache[32];
	double timestamp;
};

volatile struct cache_s cache[10000]; 
volatile int cache_index = 0;
volatile int cache_flag = 0;
 
/*Functions to print error messages*/  
void error(char* msg)
{
perror(msg);
exit(0);
}
  
 /*Function to check if ip to host mapping is stored in the cache*/
char * check_ip_in_cache(char * request_url)
{
    char * temp_request_url = request_url;
    char * temp_ip = (char *) malloc(50 * sizeof(char));
    FILE *file = fopen ("ip_cache.txt", "r");
    if(file == NULL)
    {
        goto last;
    }
    printf("Reading ip cache file.......\n");
    char line[MAXBUF];
    while(fgets(line, sizeof(line), file) != NULL)
    {
    	char * token;
    	token = strtok(line, " ");
    	if(token != NULL)
    	{
    		//printf("host name is %s\n", token);
    		if(strcmp(token, request_url) == 0)
    		{
    			token = strtok(NULL, " ");
    			if(token != NULL)
    			{
    				strcpy(temp_ip, token);
    				printf("Found ip address in cache\n");
    				fclose(file);
    				return temp_ip;
    			}
    		}
    	}
    }
    fclose(file);
    last:
    return NULL; 
    
}
/*Function to check if ip is forbidden*/
int check_ip_is_forbidden(char * request_url, char * temp_ip_addr)
{
	char * temp_request_url = request_url;
    FILE *file = fopen ("forbidden_sites.txt", "r");
    if(file == NULL)
    {
        perror("File not opened :");
        exit(-1);
    }
    printf("Reading forbidden sites file.......\n");
    
    char line[MAXBUF];
    while(fgets(line, sizeof(line), file) != NULL)
    {
    	char * token;
    	//printf("line is %s\n", line);
    	token = strtok(line, "\n");
    	if(token != NULL)
    	{
    		//printf("host name or ip address is %s\n", token);
    		if(strcmp(token, request_url) == 0)
    		{
    			printf("%s: It is a forbidden site\n", token);
    			fclose(file);
    			return -1;
    		}
    		if(strcmp(token, temp_ip_addr) == 0)
    		{
    			printf("%s: It is a forbidden site\n", token);
    			fclose(file);
    			return -1;
    		}
    	}
    }
    fclose(file);
    return 0; 
}

/*Function to write ip to the cache containing ip to host mapping*/
int write_ip_to_cache(char * request_url, char * host_addr, unsigned long int length)
{
	int fd_write;
	fd_write = open( "ip_cache.txt", O_RDWR|O_CREAT|O_APPEND, 0666);
	if(fd_write == -1)
	{
		perror("File:");
		return -1;
	}
	write(fd_write, request_url, strlen(request_url));
	write(fd_write, " ", 1);
	write(fd_write, host_addr, length);
    write(fd_write, "\n", 1);  
    close(fd_write);
    return 0;     
}

/*Function to check if a cache of a certain url is present*/
int page_cache_present(char * filename)
{
	char fullpath[50] ;
	bzero(fullpath, sizeof(fullpath));
	strcpy(fullpath,"./cache/");
	//printf("full path is %s\n", fullpath);
	struct stat st = {0};
    if (stat(fullpath, &st) == -1) 
    {
        mkdir(fullpath, 0700);
        printf("directory %s is created\n", fullpath);
    }
    strcat(fullpath, filename);
    bzero(filename, sizeof(filename));
    strcpy(filename, fullpath);
    //printf("filename path is %s\n", filename);
	FILE *temp_fp = fopen(filename, "r");
    if (temp_fp == NULL)
    {
        printf("File not present\n");
        return 0;
    }
    fclose(temp_fp);
    return 1;
}

/*Function to check if timestamp subfolder is present and create a file in the timestamp folder*/ 
int timestamp_cache_present(char * filename)
{
	char fullpath[50] ;
	bzero(fullpath, sizeof(fullpath));
	strcpy(fullpath,"./timestamp/");
	//printf("full path is %s\n", fullpath);
	struct stat st = {0};
    if (stat(fullpath, &st) == -1) 
    {
        mkdir(fullpath, 0700);
        printf("directory %s is created\n", fullpath);
    }
    strcat(fullpath, filename);
    bzero(filename, sizeof(filename));
    strcpy(filename, fullpath);
    //printf("filename path is %s\n", filename);
    return 1;
}

/*Get md5um of the url*/
void check_md5sum(char * url, char * md5sum_temp)
{
    int nbytes;
    int result = 0;
    char buffer[MAXBUF];
        
    int n;
    MD5_CTX c;
    char buf[512];
    ssize_t bytes;
    unsigned char out[MD5_DIGEST_LENGTH];
    unsigned char temp[MD5_DIGEST_LENGTH];

    MD5_Init(&c);
    
    MD5_Update(&c, url, strlen(url));
    MD5_Final(out, &c);

    //printf("In md5sum function\n");

    for(n=0; n<MD5_DIGEST_LENGTH; n++)
    {   
    	char temp[3];
        sprintf(temp, "%0X", out[n]);
        strcat(md5sum_temp, temp);
    }
    return;
}

/*Check if the timestamp is expired*/
int check_for_timestamp(long int diff_time, char * filename)
{
	FILE *file = fopen (filename, "r");
    if(file == NULL)
    {
        perror("File not opened :");
        exit(-1);
    }
    printf("Reading database file.......\n");
    
    char cache_buf[MAXBUF];
    					
    while(fgets(cache_buf, sizeof(cache_buf), file) != NULL)
    {
    	char * token = strtok(cache_buf, "\n");
    	if(token != NULL)
    	{
    		puts(token);
    		
            long int diff_time_temp = 0;
            char * eptr;
			diff_time_temp = strtol(token, &eptr, 10);
			printf("timestamp value in file is %ld\n", diff_time_temp);
			if((diff_time_temp + expire_time) > (diff_time))
			{
				printf("cache is new enough\n");
				return 1;
			}
			else
			{
				printf("cache to be updated\n");
				return 0;
			}

    	}
    }
    fclose(file);
    return -1;
}

/*Main function*/
int main(int argc,char* argv[])
{ 
	pid_t pid;
	//sem_init(mutex,0,1);
	struct sockaddr_in addr_in,cli_addr,serv_addr;
	struct hostent* host;
	int sockfd,connfd;
	time_t start_time, end_time;
	double diff_time;
	start_time = time(0);
   
	if(argc<3)
	error("./proxy <port_no> <expiration time>");  
	printf("\n*****PROXY SERVER*****\n");

	expire_time = atoi(argv[2]);
	printf("\n\nExpiration time is %d\n\n", expire_time);
   
	bzero((char*)&serv_addr,sizeof(serv_addr));
	bzero((char*)&cli_addr, sizeof(cli_addr));
   
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(atoi(argv[1]));
	serv_addr.sin_addr.s_addr=INADDR_ANY;
   
  
	sockfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sockfd<0)
	error("Problem in initializing socket");
   
	if(bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0)
		error("Error on binding");
  
  
	listen(sockfd,50);

	printf("\n%s\n","Server running...waiting for connections.");
	
	int clilen=sizeof(cli_addr);
	for(; ;)
	{
		connfd=accept(sockfd,(struct sockaddr*)&cli_addr,&clilen);
   	
		if(connfd<0)
			error("Problem in accepting connection");
  
		pid=fork();
		if(pid==0)
		{
			printf("Created a new child.....\n");
			int i = 0;
			struct sockaddr_in host_addr;
			int flag=0,connfd1,n,port=0,sockfd1;
			char buffer[510],request[300],request_url[300],request_version[10];
			char* temp=NULL;
			bzero((char*)buffer,500);
			recv(connfd,buffer,500,0);
			
   
			sscanf(buffer,"%s %s %s",request,request_url,request_version);
            //printf("received is:\n");
            //printf("request:::%s\nrequest_url:::%s\nrequest_version:::%s:::\n", request, request_url, request_version);

            /*Processing the request*/
			if(((strncmp(request,"GET",3)==0))&&((strncmp(request_version,"HTTP/1.1",8)==0)||(strncmp(request_version,"HTTP/1.0",8)==0))&&(strncmp(request_url,"http://",7)==0))
			{
				int i = 0;
			    strcpy(request,request_url);
				char version[10];
				bzero(version, sizeof(version));
				memcpy(version, request_version, strlen(request_version));
				char url[300];
				bzero(url, sizeof(url));
				memcpy(url, request_url, strlen(request_url));
				printf("version is %s\n", version);
				printf("url is %s\n", url);
				
				flag=0;
   
				for(i=7;i<strlen(request_url);i++)
				{
					if(request_url[i]==':')
					{
						flag=1;
						break;
					}
					else if(request_url[i] == '?' || request_url[i] == '/')
					{
						break;
					}
				}
   
				temp=strtok(request_url,"//");
				//printf("flag is %d, temp is %s\n", flag, temp);
				if(flag==0)
				{
					port=80;
					temp=strtok(NULL,"/");
				//	printf("flag is 0, temp is %s\n", temp);
				}
				else
				{
					temp=strtok(NULL,":");
				//	printf("flag is 1, temp is %s\n", temp);
				}
   
				sprintf(request_url,"%s",temp);
				printf("host = %s",request_url);
				
				char * cache_ip = check_ip_in_cache(request_url); //check if site in cache
				//printf("version is %s\n", version);
   
				if(cache_ip == NULL)
				{
                	printf("\n...........getting ip address and storing in cache.....\n");
                	host=gethostbyname(request_url);
				
                	if(host == NULL) //check if host name exists
					{
						send(connfd,"SERVER NOT FOUND\n",17,0);
						goto closing;
					}
					cache_ip = (char *)malloc(host->h_length * sizeof(char));
					int result = write_ip_to_cache(request_url, (char *)host->h_addr, host->h_length);//create host name and ip address mapping
					if(result == -1)
					{
						printf("Could not create cache\n");
						//send(connfd,"ERROR WHILE CACHING\n",20,0);
						//goto closing;
					}
					strncpy(cache_ip, host->h_addr, host->h_length);
					//printf("host->h_addr is %s\n, cache_ip is %s\n", host->h_addr, cache_ip);
				}

                else
                {
                	printf("IP address is %s\n", cache_ip);
                }
                //printf("version is %s\n", version);
                
   
				if(flag==1)
				{
					temp=strtok(NULL,"/");
					port=atoi(temp);
				}
   
   
				strcat(request,"^]");
				//printf("\nrequest after adding ^] is %s\n", request);
				temp=strtok(request,"//");
				//printf("temp token on // is %s\n", temp);
				temp=strtok(NULL,"/");	
				//printf("temp token on / is %s\n", temp);
				if(temp!=NULL)
					temp=strtok(NULL,"^]");
				//printf("temp token on ^] is %s\n", temp);
				printf("\npath = %s\nPort = %d\n",temp,port);
                //printf("version is %s\n", version);
   
   
				bzero((char*)&host_addr,sizeof(host_addr));
				host_addr.sin_port=htons(port);
				host_addr.sin_family=AF_INET;
				//printf("host length is %d\n, our length is %d\n", host->h_length, strlen(cache_ip));
				bcopy((char*)cache_ip,(char*)&host_addr.sin_addr.s_addr,strlen(cache_ip));
				char temp_ip_addr[100];
				strcpy(temp_ip_addr, inet_ntoa(host_addr.sin_addr));
				if(check_ip_is_forbidden(request_url, temp_ip_addr) == -1) //checking if site is forbidden
			    {
			    	send(connfd,"ERROR 403 FORBIDDEN\n",20,0);
					goto closing;
			    }
   
				char md5sum_temp[32];
				bzero(md5sum_temp, sizeof(md5sum_temp));
				check_md5sum(url, md5sum_temp);//get md5sum of the url
                char cache_filename[200];
                bzero(cache_filename, sizeof(cache_filename));
                strcpy(cache_filename, md5sum_temp);
				if(page_cache_present(cache_filename) == 0) //check if cache is present for the url
				{
				    /*if not present, connect to remote server and send data and cache it*/
					new_cache: ;
					sockfd1=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
					connfd1=connect(sockfd1,(struct sockaddr*)&host_addr,sizeof(struct sockaddr));
					sprintf(buffer,"\nConnected to %s  IP - %s\n",request_url,inet_ntoa(host_addr.sin_addr));
					if(connfd1<0)
						error("Error in connecting to remote server");
   
					printf("\n%s\n",buffer);
					bzero((char*)buffer,sizeof(buffer));
					if(temp!=NULL)
						sprintf(buffer,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",temp,request_version,request_url);
						//sprintf(buffer,"GET http://%s/%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",request_url,temp,request_version,request_url);
					else
					{
						//printf("version is %s\n", version);
						sprintf(buffer,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",version,request_url);
						//sprintf(buffer,"GET http://%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",request_url,request_version,request_url);
					}
					
 
 
					n=send(sockfd1,buffer,strlen(buffer),0);
					printf("\n%s\n",buffer);
					if(n<0)
						error("Error writing to socket");
					int fd_write;
					printf("md5sum is %s\n", md5sum_temp);
					fd_write = open( cache_filename, O_RDWR|O_CREAT|O_APPEND|O_TRUNC, 0666);
				
					if(fd_write == -1)
					{
						perror("File:");
						return -1;
					}
						
					do
					{
						bzero((char*)buffer,500);
						n=recv(sockfd1,buffer,500,0);
						if(!(n<=0))
						{
							write(fd_write, buffer, n);
							send(connfd,buffer,n,0);
						}
					}while(n>0);
					end_time = time(0);
					//diff_time = difftime(end_time, start_time);
					printf("time is %ld\n", end_time);
					char diff_time_char[50];
					bzero(diff_time_char, sizeof(diff_time_char));
					sprintf(diff_time_char, "%ld", end_time);
					int fd2_write;
					char timestamp_filename[200];
                	bzero(timestamp_filename, sizeof(timestamp_filename));
                	strcpy(timestamp_filename, md5sum_temp);
                	timestamp_cache_present(timestamp_filename); //save the timestamp of the cache
					fd2_write = open(timestamp_filename, O_RDWR|O_CREAT|O_APPEND|O_TRUNC, 0666);
						
					if(fd2_write == -1)
					{
						perror("File:");
						return -1;
					}
					
					write(fd2_write, diff_time_char, strlen(diff_time_char));
					write(fd2_write, "\n", 1);
					close(fd2_write);

				}
				else
				{
                    printf("Page is present in the cache\n"); //if cache present then serve data from the cache
                    end_time = time(0);
					printf("diff in time is %ld\n", end_time);
					char diff_time_char[10];
					bzero(diff_time_char, sizeof(diff_time_char));
					sprintf(diff_time_char, "%ld", end_time);
					printf("mdsum is %s\n", md5sum_temp);
					int i = 0;
					int result = 0;
					char timestamp_filename[200];
                	bzero(timestamp_filename, sizeof(timestamp_filename));
                	strcpy(timestamp_filename, md5sum_temp);
                	/*Check for timestamp expiration according to user input*/
                	timestamp_cache_present(timestamp_filename); 
                	result = check_for_timestamp(end_time, timestamp_filename);
                	if(result == 1)
                	{
                		goto next;
                	}
                    else
                    {
                        printf("Updating the cache file and timestamp file\n");
                        goto new_cache;
                    }

                    next:;
                    FILE *file = fopen (cache_filename, "r");
    				if(file == NULL)
    				{
        				perror("File not opened :");
        				exit(-1);
    				}
    				printf("Reading Cached file.......\n");
    
    				char cache_buf[500];
    					
    				while( !feof(file))
    				{
    					bzero(cache_buf, sizeof(cache_buf));
    					int result = fread(cache_buf, 1, 500, file);
    					send(connfd,cache_buf,result,0);

    				}
    				fclose(file);
                    	
				}
				//close(connfd);
			}
			else
			{
				send(connfd,"400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED",18,0); //sending messages for bad requests. allows only GET, HTTP.1.0 , HTTP.1.1
			}
		    
			closing:
			close(sockfd1);
			close(connfd);
			close(sockfd);
            _exit(0);
		}
		else
		{
			close(connfd);
		}
	}
	return 0;
}
