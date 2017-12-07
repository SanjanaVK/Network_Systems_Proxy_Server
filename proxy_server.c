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
//#include <openssl/md5.h>
#include <sys/signal.h>

#define MAXBUF 2048
   
void error(char* msg)
{
perror(msg);
exit(0);
}
  
char * check_ip_in_cache(char * t2)
{
    char * temp_t2 = t2;
    char * temp_ip = (char *) malloc(50 * sizeof(char));
    FILE *file = fopen ("ip_cache.txt", "r");
    if(file == NULL)
    {
        perror("File not opened :");
        exit(-1);
    }
    printf("Reading ip cache file.......\n");
    char line[MAXBUF];
    while(fgets(line, sizeof(line), file) != NULL)
    {
    	char * token;
    	token = strtok(line, " ");
    	if(token != NULL)
    	{
    		if(strcmp(token, t2) == 0)
    		{
    			token = strtok(NULL, " ");
    			if(token != NULL)
    			{
    				strcpy(temp_ip, token);
    				printf("Found ip address\n");
    				return temp_ip;
    			}
    		}
    	}
    }
    return NULL; 
    fclose(file);
}

int write_ip_to_cache(char * t2, char * host_addr)
{
	int fd_write;
	fd_write = open( "ip_cache.txt", O_RDWR|O_CREAT|O_APPEND, 0666);
	if(fd_write == -1)
	{
		perror("File:");
		return -1;
	}
	write(fd_write, t2, strlen(t2));
	write(fd_write, " ", 1);
	write(fd_write, host_addr, strlen(host_addr));
    write(fd_write, "\n", 1);  
    close(fd_write);
    return 0;     
}

int main(int argc,char* argv[])
{
	pid_t pid;
	struct sockaddr_in addr_in,cli_addr,serv_addr;
	struct hostent* host;
	int sockfd,newsockfd;
   
	if(argc<2)
	error("./proxy <port_no>");  
	printf("\n*****WELCOME TO PROXY SERVER*****\n");
   
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
		newsockfd=accept(sockfd,(struct sockaddr*)&cli_addr,&clilen);
   	
		if(newsockfd<0)
			error("Problem in accepting connection");
  
		pid=fork();
		if(pid==0)
		{
			printf("Created a new child.....\n");
			struct sockaddr_in host_addr;
			int flag=0,newsockfd1,n,port=0,i,sockfd1;
			char buffer[510],t1[300],t2[300],t3[10];
			char* temp=NULL;
			bzero((char*)buffer,500);
			recv(newsockfd,buffer,500,0);
   
			sscanf(buffer,"%s %s %s",t1,t2,t3);
            printf("received is:\n");
            printf("t1:::%s\nt2:::%s\nt3:::%s:::\n", t1, t2, t3);
   
			if(((strncmp(t1,"GET",3)==0))&&((strncmp(t3,"HTTP/1.1",8)==0)||(strncmp(t3,"HTTP/1.0",8)==0))&&(strncmp(t2,"http://",7)==0))
			{
				strcpy(t1,t2);
   
				flag=0;
   
				for(i=7;i<strlen(t2);i++)
				{
					//if(t2[i]==':' || t2[i]=='?')
					if(t2[i]==':')
					{
						flag=1;
						break;
					}
					else if(t2[i] == '?')
					{
						break;
					}
				}
   
				temp=strtok(t2,"//");
				printf("flag is %d, temp is %s\n", flag, temp);
				if(flag==0)
				{
					port=80;
					temp=strtok(NULL,"/");
					printf("flag is 0, temp is %s\n", temp);
				}
				else
				{
					temp=strtok(NULL,":");
					printf("flag is 1, temp is %s\n", temp);
				}
   
				sprintf(t2,"%s",temp);
				printf("host = %s",t2);
				char * cache_ip = NULL;
				//check_ip_in_cache(t2);
				if(cache_ip == NULL)
				{
                	printf("getting ip address.....\n");
                	host=gethostbyname(t2);
				
                	if(host == NULL)
					{
						send(newsockfd,"SERVER NOT FOUND\n",17,0);
						goto closing;
					}
					cache_ip = (char *)malloc(host->h_length * sizeof(char));
					int result = write_ip_to_cache(t2, (char *)host->h_addr);
					if(result == -1)
					{
						printf("Could not create cache\n");
						//send(newsockfd,"ERROR WHILE CACHING\n",20,0);
						//goto closing;
					}
					strncpy(cache_ip, host->h_addr, host->h_length);
					printf("host->h_addr is %s\n, cache_ip is %s\n", host->h_addr, cache_ip);
				}

                else
                {
                	printf("IP address is %s\n", cache_ip);
                }
				if(flag==1)
				{
					temp=strtok(NULL,"/");
					port=atoi(temp);
				}
   
   
				strcat(t1,"^]");
				printf("\nt1 after adding ^] is %s\n", t1);
				temp=strtok(t1,"//");
				printf("temp token on // is %s\n", temp);
				temp=strtok(NULL,"/");	
				printf("temp token on / is %s\n", temp);
				if(temp!=NULL)
					temp=strtok(NULL,"^]");
				printf("temp token on ^] is %s\n", temp);
				printf("\npath = %s\nPort = %d\n",temp,port);
   
   
				bzero((char*)&host_addr,sizeof(host_addr));
				host_addr.sin_port=htons(port);
				host_addr.sin_family=AF_INET;
				bcopy((char*)host->h_addr,(char*)&host_addr.sin_addr.s_addr,host->h_length);
   
				sockfd1=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
				newsockfd1=connect(sockfd1,(struct sockaddr*)&host_addr,sizeof(struct sockaddr));
				sprintf(buffer,"\nConnected to %s  IP - %s\n",t2,inet_ntoa(host_addr.sin_addr));
				if(newsockfd1<0)
					error("Error in connecting to remote server");
   
				printf("\n%s\n",buffer);
				//send(newsockfd,buffer,strlen(buffer),0);
				bzero((char*)buffer,sizeof(buffer));
				if(temp!=NULL)
					sprintf(buffer,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",temp,t3,t2);
					//sprintf(buffer,"GET http://%s/%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",t2,temp,t3,t2);
				else
					sprintf(buffer,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",t3,t2);
					//sprintf(buffer,"GET http://%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",t2,t3,t2);
 
 
				n=send(sockfd1,buffer,strlen(buffer),0);
				printf("\n%s\n",buffer);
				if(n<0)
					error("Error writing to socket");
				else
				{
					do
					{
						bzero((char*)buffer,500);
						n=recv(sockfd1,buffer,500,0);
						if(!(n<=0))
						send(newsockfd,buffer,n,0);
					}while(n>0);
				}
				//close(newsockfd);
			}
			else
			{
				send(newsockfd,"400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED",18,0);
			}
			closing:
			close(sockfd1);
			close(newsockfd);
			close(sockfd);
            _exit(0);
		}
		else
		{
			close(newsockfd);
		}
	}
	return 0;
}
