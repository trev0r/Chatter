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

#define PORT "1440" // the port client will be connecting to 
#define CHATPORT "1441" //port that clients communicate with eachother on
#define MAXDATASIZE 50 // max number of bytes we can get at once 
#define MAXUSERNAMESIZE 15 //max username size
#define BACKLOG 10   // how many pending connections queue will hold 

#define STDIN 0 //file descriptor for standard input


struct _client
{
	bool connected;
	struct sockaddr_in *address;
	int socket_data;
	int address_length;
	char username[MAXUSERNAMESIZE];
};


/**
Global Variables
**/
bool clientrunning;
int sockfd = 0;		//server socket fd
int chatsockfd = 0; // chatsocket fd
int listener = 0; //listener socked fd

fd_set masterFD; //master list of all active fds
fd_set checkFD; //temp copy
int maxFD = -1; //largest fd
_client *current_chat ;
string username;




// get sockaddr, IPv4 or IPv6: 
void *get_in_addr(struct sockaddr *sa) 
{ 
    if (sa->sa_family == AF_INET) { 
        return &(((struct sockaddr_in*)sa)->sin_addr); 
    } 
    return &(((struct sockaddr_in6*)sa)->sin6_addr); 
} 

void printhelp()
{
	cout << "/g\t\t\t: (g)et list of who is online\n";
	cout << "/t user\t\t\t: (t)alk. Initiate chat with user\n";
	cout << "/b\t\t\t: (b)yebye. Close chat with user\n";
	cout << "/h\t\t\t: help\n";
	cout << "/q\t\t\t: quit\n";
}


void accept_connection()
{
    int numbytes;
    char buffer[MAXUSERNAMESIZE];
    char s[INET_ADDRSTRLEN];
    _client *temp_chat = new (_client);
    sockaddr_in *their_addr;
    their_addr = new (sockaddr_in);
    socklen_t sin_size; 
    int new_fd;
    string response;
    sin_size = sizeof *their_addr;
    

    if((new_fd = accept(listener, (struct sockaddr *)their_addr, &sin_size)) == -1) //accept connection
	    perror("accept");
    else
    {
	
            temp_chat->connected = true;
            temp_chat->address = their_addr;
            temp_chat->socket_data = new_fd;
            temp_chat->address_length = sin_size;
            
            // add new connection to set
            FD_SET(new_fd, &masterFD);
            if(new_fd > maxFD)
                maxFD = new_fd;
                
             //get username
			if ((numbytes = recv(temp_chat->socket_data, buffer, sizeof buffer, 0)) == -1)
			{
			    perror("recv");
			}
			else
			{
			buffer[numbytes] = '\0';
			for (int j = 0; j < MAXUSERNAMESIZE; j++)
			{
				temp_chat->username[j] = buffer[j];
			}
			//ask to accept chat. If 'yes', send confirmation, if 'no' close socket.
		    cout << buffer << " would like to chat. Type 'yes' to accept or 'no' to decline: ";
			cin >> response;
	
			while(!(response == "yes" ||response == "no"))
			{
			   cout << "invalid response, please type either 'yes' or 'no': ";
			   cin >> response;
			}
			//yes response
			if(response == "yes")
			{
			    current_chat = temp_chat;
			    cout << "Now chatting with " << current_chat->username << endl;
			    if(send(current_chat->socket_data, username.c_str(),  username.length(), 0) == -1)
			        perror("send");
			}
			//no response
			if (response == "no"){
			    close(new_fd);
			    return;
			    }
			    
				    
				
			}
    }
	
}


//accept msg sent from already connected user and closes connection if they disconnected.
void receive_msg()
{
    char buffer[MAXDATASIZE];
    int numbytes;
    if((numbytes = recv(current_chat->socket_data, buffer, sizeof buffer,0)) <= 0)
    {
        if(numbytes == 0)
        {
            current_chat->connected = false;
            FD_CLR(current_chat->socket_data, &masterFD);
            cout << current_chat->username << " has disconnected." << endl;
        }
        else    
            perror("recv");
    }
    else
    {
        buffer[numbytes] = '\0';
        cout << current_chat->username << ": " << buffer<< endl;
    }
}


int main(int argc, char *argv[]) 
{ 
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    char uname[MAXUSERNAMESIZE];

    struct addrinfo hints, *servinfo, *p; 
    int rv;
	string currentconvo, servername;
	int len,i;
	int yes = 1;
    char s[INET6_ADDRSTRLEN];
 
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;  

       //Set up to detect key press
    fd_set kp_set;
    fd_set temp_kp_set;
    FD_ZERO(&kp_set);
    FD_SET(STDIN,&kp_set);
    
    
    
    //Start of interactive terminal
    cout << "Chat Client Started." <<endl;
    cout << "Server:";
    cin >> servername;
	cout << "Username: "; 
	cin >> username;
	while(username == "/h")
	{
	    printhelp();
	    cout << "Username: ";
	    cin >> servername;
	}
	while(username.length() > MAXUSERNAMESIZE){
	    cout << "Username is too long, try again or type /q to quit." <<endl;
	    cin >> username;
	    if(username == "/q")
	        exit(0);
	} 
	   
	len = username.length();
	
	//connect to server
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM; 
    if ((rv = getaddrinfo(servername.c_str(), PORT, &hints, &servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return 1; 
    } 
    // loop through all the results and connect to the first we can 
    for(p = servinfo; p != NULL; p = p->ai_next) { 
        if ((sockfd = socket(p->ai_family, p->ai_socktype, 
                p->ai_protocol)) == -1) { 
            perror("client: socket"); 
            continue; 
        } 
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd); 
            perror("client: connect"); 
            continue;
		} 
		break; 
	} 
	if (p == NULL) { 
		fprintf(stderr, "client: failed to connect\n"); 
		return 2; 
	} 
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), 
		      s, sizeof s); 
	printf("client: connecting to %s\n", s); 

    //send username
   	if(send(sockfd, username.c_str(), len, 0) == -1){
	  perror("username");
	  exit(1);
	}
    if(recv(sockfd, buf, sizeof buf,0) !=-1 )
    {
        if(buf[0] == '/')
        {
            cout << "Invalid Username: Server Ended Connection. Quiting." << endl;
            exit(1);
        }
    }
	
	
	//sets up listener for other clients to connect
	memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET; //IPv4. we may want to allow support for IPv6 too - Yink
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE; // use my IP 
    if ((rv = getaddrinfo(NULL, CHATPORT, &hints, &servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return 1; 
    } 
    
	// loop through all the results and bind to the first we can 
    for(p = servinfo; p != NULL; p = p->ai_next) { 
        if ((listener = socket(p->ai_family, p->ai_socktype, 
                p->ai_protocol)) == -1) { 
            perror("server: socket"); 
            continue; 
        } 
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, 
                sizeof(int)) == -1) { 
            perror("setsockopt"); 
            exit(1); 
        } 
        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) { 
            close(listener); 
            perror("server: bind"); 
            continue; 
        } 
        break; 
    } 
	// cannot find address to bind to
    if (p == NULL)  { 
    fprintf(stderr, "server: failed to bind\n"); 
    return 2;
	}
	 
	freeaddrinfo(servinfo); // all done with this structure 
	
	
	if (listen(listener, BACKLOG) == -1) { 
		perror("listen"); 
		exit(1); 
	} 
	
	
	
	FD_SET(listener, &masterFD); //add listener to set
	maxFD = listener; //listener is maxFD since it is first
	
	//current_chat is the person you are currently chatting with, initialize to not connected.
    current_chat = new(_client);
    current_chat->connected= false;
    
	
    clientrunning = true;

    
	cout << "Connected to " << s << " type /h for help\n";
    //loop until user quits
    while(clientrunning)
    {  
    
    
        //check for any new connections or messages to receive
 
       //keypress condition
       temp_kp_set = kp_set;
       select(STDIN+1, &temp_kp_set, NULL, NULL, &tv);
       
       if(FD_ISSET(STDIN,&temp_kp_set))
       {
        //get input from terminal
        cin.getline(buf, MAXDATASIZE);
        //this does all the processing for server commands
        if(buf[0] == '/')
        {
            if(buf[1] == '\0')
                cout << "Bad Command." << endl;
            else if(buf[1] == 'q') //performs quit
            {
                if(send(sockfd, buf, sizeof buf, 0) == -1)
                    perror("/q send error");
                if((numbytes = recv(sockfd, buf, sizeof buf,0)) == -1)
                    perror("/q recv error");
                buf[numbytes] = '\0';
                cout << buf << endl;
          
                clientrunning = false;
            }
            else if(buf[1] == 'g') //obtains list of connected users
            {
                if(send(sockfd, buf, sizeof buf, 0) == -1)
                    perror("/g send error");
                if((numbytes = recv(sockfd, buf, sizeof buf, 0)) == -1)
                    perror("/g recv error");
                buf[numbytes] = '\0';
                
                cout << "Available Users:" <<endl;
                cout << buf << endl;
            }
            else if(buf[1] == 't') //allows user to connect to any other available user
            {
          
                if(send(sockfd, buf, sizeof buf, 0) == -1)
                    perror("/t send error");
        
                if((numbytes = recv(sockfd, buf, sizeof buf, 0)) == -1)
                    perror("/t recv error");
                buf[numbytes] = '\0';
                if(buf[0] == '/')
                    cout << "Invalid username input, cannot connect" <<endl; 
                else
                {
                    cout << "attempting to connect to " << buf << endl;

                    //all the stuff you need to do to connect
                    memset(&hints, 0, sizeof hints); 
                    hints.ai_family = AF_UNSPEC; 
                    hints.ai_socktype = SOCK_STREAM; 
                    if ((rv = getaddrinfo(buf, CHATPORT, &hints, &servinfo)) != 0) { 
                        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
                        return 1; 
                    } 
                    // loop through all the results and connect to the first we can 
                 
                    for(p = servinfo; p != NULL; p = p->ai_next) 
                    { 
                        if ((chatsockfd = socket(p->ai_family, p->ai_socktype, 
                            p->ai_protocol)) == -1)
                        { 
                            perror("client: socket"); 
                            continue; 
                        } 
                        if (connect(chatsockfd, p->ai_addr, p->ai_addrlen) == -1) 
                        { 
                            close(chatsockfd); 
                            perror("client: connect"); 
                            continue;
    		            } 

    	            	break; 
    	            } 
    	            if (p == NULL)
    	            { 
    		            fprintf(stderr, "client: failed to connect\n"); 
    		            return 2; 
    	            } 
    	            else
    	            {
    	         
    	                if(send(chatsockfd, username.c_str() , username.length(), 0) == -1)
    	                    perror("send");
    	                if((numbytes = recv(chatsockfd, uname, sizeof uname, 0)) <= 0)
    	                {
    	                    if(numbytes == 0)
    	                        cout << "User does not want to chat." << endl;
    	                    else
    	                        perror("recv");
    	                }                 
    	                else
    	                {
    	                uname[numbytes] = '\0';
    	                current_chat->socket_data = chatsockfd;
    	                current_chat->connected = true;
    	                FD_SET(current_chat->socket_data, &masterFD);
    	                if(current_chat->socket_data > maxFD)
                            maxFD = current_chat->socket_data;
    	                for (int j = 0; j < MAXUSERNAMESIZE; j++)
    			        {
    					    current_chat->username[j] = uname[j];
    				    }
    					
    				    
    	             
    	                }
    	            }
	            }   
            }
            else if(buf[1] == 'b') //ends conversation with specified user
            {
                if(current_chat->connected)
                {
                    cout << "Disconnecting from " << current_chat->username << endl;
                    FD_CLR(current_chat->socket_data, &masterFD);
                    current_chat->connected = false;
                    close(current_chat->socket_data);
                }
                else
                    cout << "No one to disconnect from." <<endl;
           
            }
            else if(buf[1] == 'h') //provides list of available commands
            {
				printhelp();
            }
            else
                cout<< "Bad Command." <<endl;
     
       }
       //handle messages being sent
       else 
       {
        if(current_chat->connected)
        {
            if(send(current_chat->socket_data, buf, sizeof buf, 0) == -1)
            {
                current_chat -> connected = false;
                cout << current_chat->username << " has disconnected." <<endl;
             
            }
            else
                cout << username << ": "<< buf<<endl;
        }
       
       }
     }
     else{
           checkFD = masterFD;
       if (select(maxFD+1, &checkFD, NULL, NULL, &tv) == -1)
	   {
            perror("select");
            exit(4);
       }
       else
       {
            for(i = 0; i <= maxFD; i++)
            {
                if(FD_ISSET(i, &checkFD)) //found a connection
                {     
                    if(i == listener)
                    {
                   
                        accept_connection();
                    
                    }
                    else
                    {
                    receive_msg();
                    }
                }
            }
       }
       }
       //cout << username << ": ";
   }
              

	 

    if(current_chat->connected)
    {
        cout << "Disconnecting from " << current_chat->username << endl;
        current_chat->connected = false;
        close(current_chat->socket_data);
    }
	close(sockfd); 
	return 0; 
} 
