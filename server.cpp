/**
 * Task: Client/Server - File Transmissions
 * Description: Server
 * Author: Martin Kopec
 * Login: xkopec42
 * Date: 20.04.2016
 */

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>  //inet_addr
#include <thread>       //-std=c++0x -pthread
#include <mutex>
#include <stack>
#include <sstream>
//#include <sys/sendfile.h>     //freeBSD does not support

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define MAX_BUFF_SIZE 4096
#define MAX_CLIENTS 5

/*Globals declarations*/
std::mutex threadMtx;   //mutex for push/pop operation
std::stack<int> numberOfThreads;

using namespace std;

/*Enum for identifying and result of transfer operation*/
enum ReqAns{
    Up,         //upload operation, from client side
    Down,       //download operation, form client side
    ACK,        //operation completed successfully
    NACK,       //operation failed
    NotFound,   //requested file not found
    Unknown,    //unrecognized request
    TooLong,    //too long request, longer than MAX_BUFF_SIZE
    Incomplete, //in case required attribute missing (Length: / File:)
    Overloaded  //MAX_CLIENTS clients are connected
};

/*--------Prototypes---------*/
int bindOp(unsigned short int port, int socket_desc);
int sendResponse(int socket, ReqAns type, string customMsg);
void handleClient(int *comm);
int receiveReq(int socket, char *buffer);
long fileSizeFunc(string filename);
string parseRequest(int comm_socket, string toFind, string request);
string parseFilename(string path);
void upload(int comm_socket, string request);
void download(int comm_socket, string request);

int main(int argc, char *argv[]) {
    int welcoming_socket;
    unsigned short int port;

    //check arguments
    if(strcmp(argv[1],"-h") == 0 || strcmp(argv[1], "--help") == 0){
        cout << "\nHELP:\n    ./server -p <port_number>\n\n";
        return EXIT_SUCCESS;
    }

    if(argc < 3 || argc > 3 || strcmp(argv[1], "-p")){
        cerr << "Wrong arguments" << endl;
        return EXIT_FAILURE;
    }
    istringstream (argv[2]) >> port; // check if range?

    //create socket
    if((welcoming_socket = socket(PF_INET6, SOCK_STREAM, 0)) == -1){
        cerr << "Opening socket FAILED" << endl;
        return EXIT_FAILURE;
    }
    int no = 0;
    setsockopt(welcoming_socket, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no));

    //bind port, socket
    if(bindOp(port, welcoming_socket) == EXIT_FAILURE){
        return EXIT_FAILURE;
    }

    //listen - makes passive socket
    if(listen(welcoming_socket, 1) == -1){
        cerr << "Listen operation FAILED" << endl;
        return EXIT_FAILURE;
    }

    //declarations for accept function
    struct sockaddr_in6 client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while(1) {

        int comm_socket = accept(welcoming_socket, (struct sockaddr *) &client_addr, &client_addr_len); //-1

        if(comm_socket < 0){
            cerr << "ERROR: Bad socket of new connection" << endl;
        }
        else{   //new thread here

            threadMtx.lock();
            if (numberOfThreads.size() == MAX_CLIENTS){
                cerr << "Maximum connections reached" << endl;
                sendResponse(comm_socket, Overloaded, "");  //inform client about situation
            }
            std::thread t1(&handleClient, &comm_socket);
            numberOfThreads.push(1); // push something, no matter what, important is just the number of items
            t1.detach();        //makes thread independent, when ends, his memory is freed automatically
            threadMtx.unlock();
        }
    }

    return EXIT_SUCCESS;
}
/**
 * @description - Bind socket to the address
 * @param unsigned short int port - port on which server will listen
 * @param int socket_desc - opened socket for communication with clients
 * @return int - success = 0, failure = 1
 */
int bindOp(unsigned short int port, int socket_desc) {
    struct sockaddr_in6 addrIPv6;

    //fill structure for bind
    memset(&addrIPv6, 0, sizeof(addrIPv6));
    addrIPv6.sin6_family = AF_INET6;
    addrIPv6.sin6_addr = in6addr_any;
    addrIPv6.sin6_port = htons(port);

    if( bind(socket_desc, (struct sockaddr*) &addrIPv6, sizeof(addrIPv6)) == -1){
        cerr << "Bind operation FAILED" << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @description - Send response to the client, f.e.: information about success of operation
 * @param int socket - opened socket for communication with specific client
 * @param ReqAns type - enum, type/result of operation
 * @param string customMsg - ReqAns type send another information, f.e.: length of data
 * @return int - success = 0, failure = 1
 */
int sendResponse(int socket, ReqAns type, string customMsg) {
    const char *c;
    unsigned long responseLen;

    ostringstream strType;      //because of FreeBSD otherwise to_string(type) would be enough
    strType << type;

    customMsg = strType.str()+customMsg+"\n\n";
    responseLen = customMsg.length()+1;
    c = customMsg.c_str();

    int sent = 0;
    int bytes = 0;

    while (sent < (int)responseLen){
        bytes = (int) send(socket, c+sent, (size_t) responseLen, 0);
        if (bytes < 0) {
            cerr << "ERROR: Sending operation: " << type << " FAILED" << endl;
            return EXIT_FAILURE;
        }
        if (bytes == 0) {
            break;
        }
        sent += bytes;
    }
    if(sent != (int)responseLen){
        cerr << "ERROR: NOT entire response NO " << type << " sent" << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @description - According to client's request decide which operation to handle
 * @param int * comm - pointer to opened socket to client
 * @return void
 */
void handleClient(int *comm) {

    int comm_socket = *comm;
    char buffer[MAX_BUFF_SIZE]; //load request here

    int ReqType = 0;
    if((ReqType = receiveReq(comm_socket, buffer)) == -1){
        return;
    }

    string str(buffer);

    switch (ReqType){
        case Up:
            upload(comm_socket, str);
            break;
        case Down:
            download(comm_socket, str);
            break;
        default:
            cerr << "UNKNOWN request received" << endl;
            sendResponse(comm_socket, Unknown, "");  //inform client that unrecognized request was received
    }

    threadMtx.lock();
    numberOfThreads.pop();  //pop one item
    threadMtx.unlock();
    close(comm_socket);
}

/**
 * @description - Receive and parse request, send response back if wrong request received
 * @param int socket - opened socket to the client
 * @param char *buffer - pointer to buffer where client's request is stored
 * @return int - ReqAns number when success otherwise -1
 */
int receiveReq(int socket, char *buffer) {
    memset(buffer, 0, MAX_BUFF_SIZE);
    bool entireMsg = false;
    int received = 0;
    int total = 0;

    while(true){

        received =(int) recv(socket, buffer + total, (size_t)(MAX_BUFF_SIZE - total), 0);

        string str(buffer);
        if(str.find("\n\n") != string::npos){
            entireMsg = true;
            break;
        }
        if (received <= 0){
            break;
        }
        total += received;

        if(total >= MAX_BUFF_SIZE){ // too long request
            sendResponse(socket, TooLong, "");
            return -1;
        }
    }
    if(!entireMsg){
        sendResponse(socket, NACK, "");
        return -1;
    }

    return buffer[0] - '0';       //return reqAns type
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
        return -1;      //but file should exists, it's checked in other context
    }
    file.seekg(0, ios::end);
    streamoff fileSize = file.tellg();
    file.close();
    return fileSize;
}

/**
 * @description - Get filename or length of file from request
 * @param int comm_socket - opened socket to the client
 * @param string toFind - the attribute to find for in request and obtain his value
 * @param string request - request to be searched for attribute
 * @return string - the value of attribute f.e.: filename, length of file
 */
string parseRequest(int comm_socket, string toFind, string request) {

    size_t index = request.find(toFind);
    if(index == string::npos){
        sendResponse(comm_socket, Incomplete, "");
        return "";
    }
    size_t end = (request.substr(index)).find("\n");
    return request.substr(index + toFind.length(), end - toFind.length());
}
/**
 * @description - Get filename if path was given
 * @param string path - file path
 * @return string - return just name of file
 */
string parseFilename(string path){

    if(path.find_last_of("\\/") != string::npos){
        path = path.substr(path.substr(0, path.find_last_of("\\/")).length()+1);
    }

    return path;
}

/**
 * @description - Handle upload (from client's side) operation
 * @param int comm_socket - opened socket to the client
 * @param string request - client's request
 * @return void
 */
void upload(int comm_socket, string request) {

    int bytes = 0;
    int received = 0;
    char buffer[MAX_BUFF_SIZE];
    long dataLength;
    string filename;
    ofstream file; //has automatically ios::out flag

    //get filename
    if((filename = parseRequest(comm_socket, "File:", request)) == ""){
        return;
    }
    filename = parseFilename(filename);

    //get length of data
    string temp;
    if((temp = parseRequest(comm_socket, "Length:", request)) == ""){
        return;
    }
    istringstream ss(temp);
    ss >> dataLength;

    //open file
    file.open(filename, ios::binary);
    if( !file.is_open() ){
        cerr << "Unable to create a file" << endl;
        sendResponse(comm_socket, NACK, "");
        return;
    }

    //inform client,that upload request received and handled successfully
    sendResponse(comm_socket, ACK, "");

    //receive and write data out
    while(bytes != dataLength) {

        memset(buffer, 0, sizeof(buffer));

        received = (int) recv(comm_socket, buffer, MAX_BUFF_SIZE, 0);

        if (received <= 0) {
            break;
        }
        file.write(buffer, received);
        bytes += received;
    }

    file.close(); //check if successful?
    if(bytes == dataLength){ sendResponse(comm_socket, ACK, ""); }
    else{ sendResponse(comm_socket, NACK, ""); } //delete created file??
}

/**
 * @description - Handle download (from client's side) operation
 * @param int comm_socket - opened socket to the client
 * @param string request - client's request
 * @return void
 */
void download(int comm_socket, string request) {

    string filename;
    int upload_file;
    long dataLength;

    //get filename
    if((filename = parseRequest(comm_socket, "File:", request)) == ""){
        return;
    }
    filename = parseFilename(filename);

    //check if exists
    upload_file = open(filename.c_str(), O_RDONLY); //flock ???
    if(upload_file == -1){
        sendResponse(comm_socket, NotFound, "");    //inform client
        return;
    }
    //Send ACK and length of file
    dataLength = fileSizeFunc(filename);
    ostringstream strData;  //because of freeBsd otherwise to_string(dataLength) would be enough
    strData << dataLength;
    sendResponse(comm_socket, ACK, "\nLength:"+strData.str());

    /* if(sendfile(comm_socket, upload_file, 0, (size_t)dataLength) == -1){      //on FreeBSD can't be used
         cerr << "Sendfile function FAILED" << endl;
         return;
     }*/

    char buffer[MAX_BUFF_SIZE];
    ssize_t bytes_read = 0;
    ssize_t bytes_written = 0;
    ssize_t total = 0;

    while (1) {

        memset(buffer, 0, sizeof(buffer));

        bytes_read = read(upload_file, buffer, sizeof(buffer));

        if (bytes_read == 0) { break; } //whole file is read

        if (bytes_read < 0) {
            cerr << "Reading from file FAILED" << endl;
            return;
        }

        char *buffPtr = buffer;
        while (bytes_read > 0) {
            bytes_written = write(comm_socket, buffPtr, (size_t) bytes_read);
            if (bytes_written <= 0) {
                cerr << "Sending bytes FAILED" << endl;
                return;
            }
            bytes_read -= bytes_written;
            buffPtr += bytes_written;
            total += bytes_written;
        }

    }
    if(total != dataLength){ cerr << "Not entire data sent: " << total << " - " << dataLength << endl; }

    close(upload_file);

}