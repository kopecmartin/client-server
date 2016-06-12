#!/bin/bash

###
# Description: Test script for project: Files Transmission
# Author: Martin Kopec
# Login: xkopec42
# Date: 23.04.2016
###


#compile .cpp sources
make

#create direcotry with file to transport
if [ -d "testFolder" ]; then

    rm -r testFolder

    mkdir -p testFolder
    if [ $? -ne 0 ]; then
        echo "Testing terminated, because testFolder folder was not created, probably because of rights"
        exit 1
    fi

else

    mkdir -p testFolder
    if [ $? -ne 0 ]; then
        echo "Testing terminated, because testFolder folder was not created, probably because of rights"
        exit 1
    fi

fi

echo "This is content of a file, which will be transferred" > ./testFolder/fileToTransport
echo "This is content of a file, which will be downloaded" > fileToDownload



#run server
./server -p 12241 &

TASK_PID=$!
echo "running server: $TASK_PID"


#run tests
echo "----TEST 01: Upload fileToTransport file from testFolder"
./client -p 12241 -h 127.0.0.1 -u ./testFolder/fileToTransport
echo "----TEST 01 completed"
echo "---------------------"


echo "----TEST 02: Download not existed file"
./client -p 12241 -h 127.0.0.1 -d notExistedFile123587
echo "----TEST 02 completed"
echo "---------------------"



#create folder for client, because server and client can't be in the same location, it would be make no sense
if [ -d "clientDir" ]; then
    rm -r clientDir
    
    mkdir -p clientDir
    if [ $? -ne 0 ]; then
        echo "Testing terminated, because testFolder folder was not created, probably because of rights"
        exit 1
    fi

else 

    mkdir -p clientDir
    if [ $? -ne 0 ]; then
        echo "Testing terminated, because testFolder folder was not created, probably because of rights"
        exit 1
    fi

fi 


cp client ./clientDir/      #copy client to the folder
cd ./clientDir/

#run test
echo "----TEST 03: Download fileToDownload file"
./client -p 12241 -h 127.0.0.1 -d fileToDownload 
echo "----TEST 03 completed"
echo "---------------------"

cd ../


#kill server process
kill $TASK_PID #>/dev/null


#clean all created files
make clean >/dev/null

rm -r testFolder
rm -r clientDir
rm fileToTransport
rm fileToDownload
