#Client/Server


##About
Applications were implemented in C++ using sockets API as a school project. Applications were developed and tested for FreeBSD server.

Location of files on server side is in server's current directory (directory where is server running from ). Downloaded file is saved into client's current directory. 


**Limitations of the server**
- max length of request is 4096 bytes
- can handle max 5 clients simultaneously


##Protocol description
Protocol message consists of attributes divided by newline character. At the end of a message is sequence of two newline characters.

**Request**  
First attribute has to be type of operation to be executed.
- 0 (request to upload file)
  - required attributes:
    - File:(file name)
    - Length:(length of file to be uploaded in bytes)

- 1 (request to download file)
  - required attributes:
    - File:(file name)

**Server response**
- 2 (request was successfully accepted / upload was successful)
- 3 (NACK = unsuccessful upload operation)
- 4 (File was not found)
- 5 (Unknown request)
- 6 (Request is too long)
- 7 (Missing required attribute)
- 8 (Server is overloaded at the moment)

**Request example for upload operation**  
0\n  
File:file.txt\n  
Length:42\n  

**Request example for download operation**  
1\n  
File:file.txt\n  


##Run the server
```
make server
./server -p <port number, where server will expect a connection>
```


##Run the client
```
make client
./client -p <port number, where client will create a connection> -h <server host name / IP address>
```