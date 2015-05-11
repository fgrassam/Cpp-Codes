#include <iostream>
#include <sys/socket.h>
#include <fstream>
#include <string>
#include <cstring>
#include <netdb.h>      // gethostbyname
#include <unistd.h>     // Close file discriptors // socketfd
#include <stdlib.h>
#include <sys/time.h>   // for timeval
#include <assert.h>

void error(const char *msg);

int main()
{
    int n_sockfd, n_addinf, n_connect, n_sockopt;
    struct addrinfo add_hints;
    struct addrinfo *add_results, *rp;
    std::string params[4];
    std::ifstream paramsFile ("/home/frassam/Documents/params.txt");
    /* File Structure is:
    Line 1 : Server address
    Line 2 : Port number
    Line 3 : Username
    Line 4 : Password
    */

    struct timeval sockTimeout;
    sockTimeout.tv_sec = 5;
    sockTimeout.tv_usec = 0;

    if (paramsFile.is_open())
    {
        for (int i=0; i<4; i++)
        {
            if (not std::getline(paramsFile,params[i]))
            {
                //std::string errString = "Could not read parameter " + std::to_string(i) + '\n';
                std::string errString = "Could not read parameter \n";
                const char * c = errString.c_str();
                error(c);
                exit(0);
            }
            std::cout << params[i] << '\n';
        }
    }
    else std::cout << "Cannot open file...";

    // Get address information
    memset(&add_hints, 0, sizeof(struct addrinfo));     // Prefills the memory location
    add_hints.ai_family = AF_UNSPEC;
    add_hints.ai_socktype = SOCK_STREAM;
    add_hints.ai_flags = 0;
    add_hints.ai_protocol = 0;
    const char * node = params[0].c_str();
    const char * service = params[1].c_str();

    n_addinf = getaddrinfo( node,
                            service,
                            &add_hints,
                            &add_results);
    if (n_addinf != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(n_addinf));
        exit(EXIT_FAILURE);
    }

    for (rp = add_results; rp != NULL; rp = rp->ai_next)
    {
        // Openning Socket
        n_sockfd = socket(  rp->ai_family,
                            rp->ai_socktype,
                            rp->ai_protocol);
        if (n_sockfd < 0)
        {
            std::cout << "Unable to open socket\n";
            continue;
        }
        n_sockopt = setsockopt(n_sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&sockTimeout, sizeof(timeval));
        assert (n_sockopt>=0);

        std::cout << "Connecting...\n";
        n_connect = connect(n_sockfd,
                    rp->ai_addr,
                    rp->ai_addrlen);
        std::cout << "n_connect is : " << n_connect << '\n';
        if (n_connect != -1)
        {
            std::cout << "Connection successfully established ...\n";
            break;  // A connection was successfully established no need to try more
        }

        close(n_sockfd);    // Will only be closed if a connection was not successful
    }
    if (rp == NULL)
        error("Was not able to establish a connection.... exiting....\n");

    freeaddrinfo(add_results);
}

void error(const char *msg)
{
    perror(msg);
    exit(0);
}
