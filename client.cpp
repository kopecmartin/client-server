/**
 * Task: Client/Server - File Transmissions
 * Description: Client
 * Author: Martin Kopec
 * Login: xkopec42
 * Date: 10.04.2016
 */

#include <iostream>
#include <sys/socket.h>
#include <getopt.h>
#include <string.h>
#include <netinet/in.h>
#include <fstream>
#include <arpa/inet.h> //inet_addr
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <netdb.h> //gethostbyname
//#include <sys/sendfile.h>     //not supported on freeBSD

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define MAX_BUFF_SIZE 4096

using namespace std;

/*Enum for identifying and result of transfer operation*/
enum ReqAns{
    Up,         //upload operation
    Down,       //download operation
    ACK,        //operation completed successfully
    NACK,       //operation failed
    NotFound,   //if requested file wasn't found
    Unknown,    //if server received unrecognized request
    TooLong,    //request was too long
    Incomplete, //request was incomplete
    Overloaded  //server is overloaded
};

/*--------Prototypes---------*/
int ipv6Connection(string host, unsigned short int port, int *socket_desc);
int createConnection(string host, unsigned short int port, int *socket_desc);
long fileSizeFunc(string filename);
int sendRequest(int socket, string request);
int receiveResponse(int socket, char *buffer);
int download(int socket_desc, string request, string filename);
int upload(int socket_desc, string request, string filename);

int main(int argc, char *argv[]) {
    int socket_desc = 0;
    unsigned short int port = 0;
    string host;
    string filename;
    bool p, h, d, u;
    p = h = d = u = false;

    //check arguments -p -h; optional -d -u
    int option;
    opterr = 0; // getopt will not print it's error messages
    while ((option = getopt(argc, argv, "p:h:d:u:")) != -1) {
        switch (option) {
            case 'p':
                try {
                    istringstream (optarg) >> port;   //check if port in range uns. short int?
                } catch (...) {
                    cerr << "-p expects number value" << endl;
                    return EXIT_FAILURE;
                }
                p = true;
                break;
            case 'h':
                host = optarg;
                h = true;
                break;
            case 'd':
                filename = optarg;
                d = true;
                break;
            case 'u':
                filename = optarg;
                u = true;
                break;
            default:
                cerr << "Wrong arguments" << endl;
                cerr << "HELP:" << endl;
                cerr << "./client -p <port_number> -h <host_addr> -d/-u <filename>" << endl;
                cerr << " -d -> download file from server\n -u -> upload file to server\n\n";
                return EXIT_FAILURE;
        }
    }
    if (!p || !h) {
        cerr << "-p and -h arguments are required" << endl;
        return EXIT_FAILURE;
    }
    if (d && u) {
        cerr << "only -d or -u argument allowed" << endl;
        return EXIT_FAILURE;
    }
    if (!d && !u) {
        cerr << "-d or -u argument is required" << endl;
        return EXIT_FAILURE;
    }

    //create connection
    if (createConnection(host, port, &socket_desc) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    //create request
    string request;
    ostringstream strOp;
    if(d){ strOp << Down; }
    else { strOp << Up;   }
    request.append(strOp.str());
    request.append("\nFile:" + filename);

    if (d) { //download

        if (download(socket_desc, request, filename) == EXIT_FAILURE) {
            return EXIT_FAILURE;
        }
    }
    else { //upload

        if (upload(socket_desc, request, filename) == EXIT_FAILURE) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

/**
 * @description - Create ipv6 connection between client and server
 * @param string host - domain name or IP address
 * @param unsigned short int port - number of port connect to
 * @param int socket_desc - opened socket through connect to the server
 * @return int - success = 0, failure = 1
 */
int ipv6Connection(string host, unsigned short int port, int *socket_desc){

    //create ipv6 socket
    if ((*socket_desc = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
        cerr << "Open socket FAILED" << endl;
    }

    struct sockaddr_in6 addr;
    struct hostent *server = NULL;

    //get IP address
    server = gethostbyname2(host.c_str(), AF_INET6);
    if (server == NULL){
        cerr << "Obtaining IP FAILED" << endl;
        return EXIT_FAILURE;
    }

    //fill structure for connect()
    memset(&addr,0,sizeof(addr));
    addr.sin6_flowinfo = 0;
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    memcpy(&addr.sin6_addr.s6_addr, server->h_addr_list[0], (size_t)server->h_length);

    if((connect( *socket_desc, (struct sockaddr*) &addr, sizeof(addr) )) < 0){
        cerr << "Unable to connect" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * @description - Create connection between client and server
 * @param string host - domain name or IP address
 * @param unsigned short int port - number of port connect to
 * @param int socket_desc - opened socket through connect to the server
 * @return int - success = 0, failure = 1
 */
int createConnection(string host, unsigned short int port, int *socket_desc) {

    if(host.find(":") != string::npos){
        return ipv6Connection(host, port, socket_desc);
    }

    //create ipv4 socket
    if ((*socket_desc = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        cerr << "Open socket FAILED" << endl;
    }

    struct hostent *server = NULL;
    struct sockaddr_in addr;

    //get IP address
    server = gethostbyname(host.c_str());
    if (server == NULL){
        cerr << "Obtaining IP FAILED" << endl;
        return EXIT_FAILURE;
    }

    //fill structure for connect()
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], (size_t)server->h_length);

    if((connect( *socket_desc, (struct sockaddr*) &addr, sizeof(addr) )) < 0){
        cerr << "Unable to connect" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * @description - Find out size of file
 * @param string filename - name of file
 * @return long - size of file in bytes
 */
long fileSizeFunc(string filename) {

    ifstream file;
    file.open(filename, ios::binary);
    if(!file){
        return -1; //but file should exists, it's checked in other context
    }
    file.seekg(0, ios::end);
    streamoff fileSize = file.tellg();
    file.close();
    return fileSize;
}

/**
 * @description - Send request to the server
 * @param int socket - opened socket to the server
 * @param string request - request to be sent
 * @return int - success = 0, failure = 1
 */
int sendRequest(int socket, string request) {

    unsigned long requestLen = request.length()+1;
    const char *c;

    c = request.c_str();
    int sent = 0;
    int bytes = 0;
    while (sent < (int)requestLen){

        bytes = (int) send(socket, c+sent, (size_t) requestLen, 0);
        if (bytes < 0) {
            cerr << "Sending request FAILED" << endl;
            return EXIT_FAILURE;
        }
        if (bytes == 0) {
            break;
        }
        sent += bytes;
    }
    if(sent != (int)requestLen){
        cerr << "ERROR: NOT entire request sent" << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @description - Receive response and determine the situation
 * @param int socket - opened socket to the server
 * @param char *buffer - pointer to memory where store received response
 * @return int - success = 0, failure = 1
 */
int receiveResponse(int socket, char *buffer) {
    memset(buffer, 0, MAX_BUFF_SIZE);
    bool entireMsg = false;
    int received =0;
    int total = 0;

    while(true){

        received =(int) recv(socket, buffer, (size_t)(MAX_BUFF_SIZE - total), 0);

        string str(buffer);

        if(str.find("\n\n") != string::npos){
            entireMsg = true;
            break;
        }
        if (received <= 0){
            break;
        }
        total += received;
    }

    if(!entireMsg){
        cerr << "NOT entire response was received" << endl;
        return EXIT_FAILURE;
    }

    int status_code = buffer[0] - '0';
    switch (status_code){
        case ACK:
            return EXIT_SUCCESS;
        case NACK:
            cerr << "Operation FAILED" << endl;
            return EXIT_FAILURE;
        case Unknown:
            cerr << "Unrecognized request" << endl;
            return EXIT_FAILURE;
        case NotFound:
            cerr << "File NOT FOUND" << endl;
            return EXIT_FAILURE;
        case TooLong:
            cerr << "Too long request" << endl;
            return EXIT_FAILURE;
        case Incomplete:
            cerr << "Incomplete request sent" << endl;
            return EXIT_FAILURE;
        case Overloaded:
            cerr << "Server is currently busy, try again" << endl;
            return EXIT_FAILURE;
        default:
            cerr << "Operation FAILED" << endl;
            return EXIT_FAILURE;
    }
}

/**
 * @description - Handle download operation
 * @param int socket_desc - opened socket to server
 * @param string request - request which will be sent to server
 * @param string filename - name of file to download
 * @return int - success = 0, failure = 1
 */
int download(int socket_desc, string request, string filename) {

    request.append("\n\n");
    if(sendRequest(socket_desc, request) == EXIT_FAILURE){
        return EXIT_FAILURE;
    }

    char buffer[MAX_BUFF_SIZE];

    if(receiveResponse(socket_desc, buffer) == EXIT_FAILURE){
        return EXIT_FAILURE;
    }

    //get file size
    string str(buffer);
    size_t index = str.find("Length:");
    size_t end = (str.substr(index)).find("\n");
    long fileSize;
    istringstream (str.substr(index + 7, end - 7)) >> fileSize;

    //receive file itself
    ofstream file; //has automatically ios::out flag
    file.open(filename, ios::binary);
    if( !file.is_open() ){
        cerr << "Unable to create a file" << endl;
        return EXIT_FAILURE;
    }

    int bytes = 0;
    int received = 0;
    while(bytes != fileSize) {

        memset(buffer, 0, sizeof(buffer));

        received = (int) recv(socket_desc, buffer, MAX_BUFF_SIZE, 0);

        if (received <= 0) {
            break;
        }
        file.write(buffer, received);
        bytes += received;
    }

    file.close(); //check if successful?
    if(bytes != fileSize){
        cerr << "Downloading FAILED, NOT entire file was downloaded" << endl;
        return EXIT_FAILURE;
    }
    else{
        return EXIT_SUCCESS;
    }
}

/**
 * @description - Handle upload operation
 * @param int socket_desc - opened socket to server
 * @param string request - request which will be sent to server
 * @param string filename - name of file to upload
 * @return int - success = 0, failure = 1
 */
int upload(int socket_desc, string request, string filename) {

    long fileSize = fileSizeFunc(filename);
    if(fileSize == -1){
        cerr << "Unable to open file or file does not exist" << endl;
        return EXIT_FAILURE;
    }
    ostringstream strSize;
    strSize << fileSize;

    request.append("\nLength:"+strSize.str()+"\n\n");

    if(sendRequest(socket_desc, request) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    char buffer[MAX_BUFF_SIZE];

    if(receiveResponse(socket_desc, buffer) == EXIT_FAILURE){
        return EXIT_FAILURE;
    }

    /*if(sendfile(socket_desc, upload_file, 0, (size_t)fileSize) == -1){    //not supported on freeBSD
        cerr << "Uploading file FAILED" << endl;
        return EXIT_FAILURE;
    }*/
    int upload_file = open(filename.c_str(), O_RDONLY); //should exist, because fileSize was successful

    ssize_t bytes_read = 0;
    ssize_t bytes_written = 0;

    while (1) {

        memset(buffer, 0, sizeof(buffer));

        bytes_read = read(upload_file, buffer, sizeof(buffer));
        if (bytes_read == 0) { break; } //whole file is read

        if (bytes_read < 0) {
            cerr << "Reading from file FAILED" << endl;
            return EXIT_FAILURE;
        }

        char *buffPtr = buffer;
        while (bytes_read > 0) {
            bytes_written = write(socket_desc, buffPtr, (size_t) bytes_read);
            if (bytes_written <= 0) {
                cerr << "Sending bytes FAILED" << endl;
                return EXIT_FAILURE;
            }
            bytes_read -= bytes_written;
            buffPtr += bytes_written;
        }
    }

    close(upload_file);  //check??

    if(receiveResponse(socket_desc, buffer) == EXIT_FAILURE){
        return EXIT_FAILURE;
    }
    cout << "Upload was successful" << endl;

    return EXIT_SUCCESS;
}