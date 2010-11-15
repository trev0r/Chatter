#include <arpa/inet.h>
#include <cstring> 
#include <errno.h> 
#include <iostream>
#include <map>
#include <netdb.h> 
#include <netinet/in.h> 
#include <signal.h> 
#include <stdio.h>
#include <stdlib.h> 
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h> 

using namespace std;

#define PORT "1440"  // the port users will be connecting to 
#define BACKLOG 10   // how many pending connections queue will hold 
#define MAXDATASIZE 50 //max characters
#define MAXCLIENTSIZE 15
#define MAXUSERNAMESIZE 15


//holds all the information necessary for clients
struct _client
{
	bool connected;
	struct sockaddr_in *address;
	fd_set fd_check;
	int socket_data;
	int address_length;
	char username[MAXUSERNAMESIZE];
};

/*
Global Variables
*/
string input;
_client clientlist[MAXCLIENTSIZE];
int onlineclients = 0;
bool continueloop;
int sockfd = 0;		//server socket fd
fd_set masterFD; //master list of all active fds
fd_set checkFD; //temp copy
int maxFD = -1; //largest fd




/*
Foward Declarations
*/
void initialize_server();
void sigchld_handler(int s) ;
bool accept_client (_client *this_client );
void accept_connection();
void receive_data();
void closeserver();

/*
Function Definitions
*/
//code that initializes server
void initialize_server()
{
	struct addrinfo hints, *servinfo, *p; 
    struct sockaddr_storage their_addr; // connector's address information 
    socklen_t sin_size; 
    struct sigaction sa; 
    int yes=1; 
    int rv; 

    FD_ZERO(&masterFD);    // clear the master and temp sets
    FD_ZERO(&checkFD);

	/*
	*	Set hints struct to call get addrinfo on current machine and pass to servinfo
	*/
	//Starts up listener
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE; // use my IP 
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
		exit(1); 
    } 
    
	// loop through all the results and bind to the first we can 
    for(p = servinfo; p != NULL; p = p->ai_next) { 
        if ((sockfd = socket(p->ai_family, p->ai_socktype, 
                p->ai_protocol)) == -1) { 
            perror("server: socket"); 
            continue; 
        } 
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, 
                sizeof(int)) == -1) { 
            perror("setsockopt"); 
            exit(1); 
        } 
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd); 
            perror("server: bind"); 
            continue; 
        } 
        break; 
    } 
	// cannot find address to bind to
    if (p == NULL)  
	{ 
	    fprintf(stderr, "server: failed to bind\n"); 
		exit(1);
	}
	 
	freeaddrinfo(servinfo); // all done with this structure 
	
	
	if (listen(sockfd, BACKLOG) == -1) { 
		perror("listen"); 
		exit(1); 
	} 
	
	
	
	FD_SET(sockfd, &masterFD); //add listener to set
	maxFD = sockfd; //listener is maxFD since it is first
	
	
	sa.sa_handler = sigchld_handler; // reap all dead processes 
	sigemptyset(&sa.sa_mask); 
	sa.sa_flags = SA_RESTART; 
	if (sigaction(SIGCHLD, &sa, NULL) == -1) { 
		perror("sigaction"); 
		exit(1); 
	}
	
	for (int i = 0; i < MAXCLIENTSIZE; i++ )  // make sure no one is connected
	{ 
		clientlist[i].connected = false; 
	} 
	printf("server: waiting for connections...\n");
}

void sigchld_handler(int s) 
{ 
    while(waitpid(-1, NULL, WNOHANG) > 0); 
} 

//helper function for accepting new client
bool accept_client (_client *this_client )
{
	sockaddr_in *their_addr;
	their_addr = new (sockaddr_in);
	socklen_t sin_size; 
	int new_fd;
	sin_size = sizeof *their_addr;
	


    if((new_fd = accept(sockfd, (struct sockaddr *)their_addr, &sin_size)) == -1)
    {
        perror("accept"); 
        delete their_addr;
        return false;
    }
    else
    {
        this_client->connected = true;
        this_client->address = their_addr;
        this_client->socket_data = new_fd;
        this_client->address_length = sin_size;

    	FD_SET(new_fd, &masterFD); //add new connection to set
    	if(new_fd > maxFD)  //check for new max socket descriptor
    	{  
    	    maxFD = new_fd;
        }
        return true;
    }
    
    return false;
   
}
//accepts connection and establishes username
void accept_connection()
{
int numbytes;
char buffer[MAXUSERNAMESIZE];
char s[INET_ADDRSTRLEN];
	if ( onlineclients < MAXCLIENTSIZE)
	{
		for ( int i = 0; i < MAXCLIENTSIZE; i++ )
		{
	
			if ( !clientlist[i].connected )
			{
				if ( accept_client ( &clientlist[i] ) )
				{
					onlineclients++;
					inet_ntop (AF_INET, &(clientlist[i].address ->sin_addr), s, INET_ADDRSTRLEN);
					
					if ((numbytes = recv(clientlist[i].socket_data, buffer, MAXUSERNAMESIZE, 0)) != -1)
					{
					    buffer[numbytes] = '\0';
					    if(buffer[0] == '/')
					    {
					        cout << "User attempted to connect with invalid username, terminating connection" << endl;
					        send(clientlist[i].socket_data, "/Error", sizeof "/Error" , 0);
					        FD_CLR(clientlist[i].socket_data, &masterFD);
					        clientlist[i].connected = false;
					        close(clientlist[i].socket_data);
					        return;
					    }
						for (int j = 0; j < MAXUSERNAMESIZE; j++)
						{
							clientlist[i].username[j] = buffer[j];
						}
						
					}
					cout << buffer << " connected from " << s << endl;
					send(clientlist[i].socket_data, "Server connection successful.", sizeof "Server connection successful.",0);
					break;
				}
			}
		}
	}
	else
	    cout<< "Too many users, no more connections allowed." << endl;
}

//receive data on socket matching current file descriptor
void receive_data(int currentFD)
{
    char buffer[MAXDATASIZE];
    char to_client[MAXDATASIZE];
    string s1, s2, username, buddylist;
    int i, j, numbytes;
    bool error = true;
    

    for(i = 0; i < MAXCLIENTSIZE; i++)
    {
        if(clientlist[i].socket_data == currentFD)
        {
            cout<< "receiving from " << clientlist[i].username << endl;
            if((numbytes = recv(clientlist[i].socket_data, buffer, MAXDATASIZE-1, 0)) <= 0)
            {   
                if(numbytes == 0) //autodisconnect
                {
                    clientlist[i].connected = false;
                    FD_CLR(clientlist[i].socket_data, &masterFD);
                    close(clientlist[i].socket_data);
                    onlineclients--;
                    cout << clientlist[i].username << " disconnected." << endl;
                }
                else
                    perror("recv");
            }
            else
            {
                buffer[numbytes] = '\0';
                if(buffer[0] == '/')
                {
                    if (buffer[1] == 'q') 		// /q = quit/disconnect from server
                    {
                        clientlist[i].connected = false;
                        if(send(clientlist[i].socket_data,"Successfully Disconnected.", sizeof "Successfully Disconnected.", 0) == -1)
                            perror("/q error");
                        FD_CLR(clientlist[i].socket_data, &masterFD);    
                        close(clientlist[i].socket_data);
                        onlineclients--;
                        cout << clientlist[i].username << " disconnected." << endl;
                    }
                    else if (buffer[1] == 'g') // /g = get buddylist
                    {
                        //find all connected users
    	                for(j = 0; j < MAXCLIENTSIZE; j++)
	                    {
	               
	                       if(clientlist[j].connected)
	                       {
	                            buddylist += clientlist[j].username;
	                            buddylist += "\n";
	                        }                           
	                    }
	                    //send them
	                    if(send(clientlist[i].socket_data, buddylist.c_str(), MAXDATASIZE-1, 0) == -1);
	                        //perror("/g send");
	                    buddylist = "";
                    }
                    else if (buffer[1] == 't')	// /t = talk. Connects to specified username in buf
                    {
                        
                        s1 = buffer;
                        cout << s1 << endl;
                        //check for no input
                        if(s1.length() < 3)
                        {
                            cout << "Invalid username" <<endl;
                          
                        }
                        else
                        {    
                            username = s1.substr(3,s1.length()-1); 
                            printf("%s is attempting to connect to user %s \n", clientlist[i].username, username.c_str());
                            //check for invalid input
                            if(username.length() > MAXUSERNAMESIZE){
                                error = true;
                                cout << "Invalid username"<< endl;
                            }
                            else
                            {   
                                error = true;
                                for(j = 0; j < MAXCLIENTSIZE; j++)
                                {
                                    //check valid input
                                    if(username == clientlist[j].username && i != j) //i != j prevents connecting to yourself
                                    {   
                                        error = false;

                                        inet_ntop(AF_INET, &(clientlist[j].address ->sin_addr) 
    												, to_client, sizeof to_client); //assumes IPV4

                                    }
                                }
                            
                            }
                            if(error)
                            {
                                cout<< "User " << username<< " does not exist." << endl;
                                if(send(clientlist[i].socket_data,"/Error", sizeof "/Error", 0) == -1)
                                    perror("/t error");
                            }
                            else
                            {
                                cout << "Connection info sent." << endl;
                                if(send(clientlist[i].socket_data,to_client, sizeof to_client, 0) == -1)
                                    perror("/t error");
                        
                            }
                        }
                    }
       
                    else
                    {
	                    perror("processing command");
                    }
                }
            }
            
        }
    }

}            
                
//does a clean shutdown of the server, closing all connections to clients.        
void closeserver()
{	
	for (int i = 0; i < MAXCLIENTSIZE; i++)
	{
		if (clientlist[i].connected)
		{
			close(clientlist[i].socket_data);
		}
	}
	close (sockfd);
}


//main program
int main(int argc, char const *argv[]) 
{ 
	initialize_server();
	continueloop = true; //loop forever
	while(continueloop) // main accept() loop 
	{   
	    //check for any new connections or data
	    checkFD = masterFD;
	    if (select(maxFD+1, &checkFD, NULL, NULL, NULL) == -1)
	    {
            perror("select");
            exit(4);
        }
		int i;
        for(i = 0; i <= maxFD; i++)
        {
            if(FD_ISSET(i, &checkFD)) //found a connection
            {     
                if(i == sockfd)
                {
                    accept_connection();
                }
                else
                {
                    receive_data(i);
                }
            }
        }
	} 
	closeserver();
	return 0; 
} 
