# Client/Server


## About
The applications were implemented in C++ using sockets API as a school project.
The applications were developed and tested on FreeBSD server.

The location of files on the server side is in the server's current directory
(directory where the server is running from ). Downloaded files are saved into
client's current directory.


**Limitations of the server**
- max length of a request is 4096 bytes
- can handle max 5 clients simultaneously


## Protocol description
The protocol message consists of attributes divided by the newline character.
A sequence of two newline characters is at the end of a message.

**Request**
The first attribute has to be a type of the operation to be executed.
- 0 (request to upload file)
  - required attributes:
    - File:(file name)
    - Length:(length of the file to be uploaded in bytes)

- 1 (request to download a file)
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


## Run the server
```
make server
./server -p <port number, where server will expect a connection>
```


## Run the client
```
make client
./client -p <port number, where client will create a connection> -h <server host name / IP address>
```
