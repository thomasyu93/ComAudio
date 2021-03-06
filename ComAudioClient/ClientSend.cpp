/*---------------------------------------------------------------------------------------
-- SOURCE FILE: clientsend.cpp
--
-- PROGRAM:     ComAudioClient
--
-- FUNCTIONS:   void ShowLastErr(bool wsa);
--              int ClientSendSetup(char* addr);
--              void ClientCleanup();
--              int ClientSend(HANDLE hFile);
--              int ClientSendMicrophoneData(HANDLE hFile);
--              DWORD WINAPI ClientSendMicrophoneThread(LPVOID lpParameter);
--              DWORD WINAPI ClientSendThread(LPVOID lpParameter);
--              void ClientCleanup(SOCKET s);
--
-- DATE:        April 2, 2016
--
-- REVISIONS:
--
-- DESIGNER:    Micah Willems, Carson Roscoe, Spenser Lee, Thomas Yu
--
-- PROGRAMMER:  Micah Willems, Carson Roscoe
--
-- NOTES:
--      This is a collection of functions for connecting and sending files to a server
---------------------------------------------------------------------------------------*/
///////////////////// Includes ////////////////////////////
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

//#include <Ws2tcpip.h>
#include <stdio.h>
#include <QDebug>
#include "Client.h"

//#pragma comment(lib, "Ws2_32.lib")

////////// "Real" of the externs in Client.h ///////////////
char address[100], p2pAddress[100];
SOCKET sendSock, p2pSendSock;
bool sendSockOpen, p2pSendSockOpen;
struct sockaddr_in server, otherClient;
char errMsg[ERRORSIZE];

/////////////////// Globals ////////////////////////////////
HANDLE hSendFile;
bool hSendOpen;

//////////////////// Debug vars ///////////////////////////
#define DEBUG_MODE
int totalbytessent;

/*---------------------------------------------------------------------------------------
--	FUNCTION:   ClientSendSetup
--
--
--	DATE:			April 7, 2016
--
--	REVISIONS:
--
--	DESIGNERS:		Micah Willems
--
--	PROGRAMMER:		Micah Willems
--
--  INTERFACE:      int ClientSendSetup(char* addr)
--
--                      char* addr: the ip address to connect to
--
--  RETURNS:        Returns 0 on success, -1 on failure
--
--	NOTES:
--      This function sets up the socket for sending a file to the server
---------------------------------------------------------------------------------------*/
int ClientSendSetup(char* addr)
{
	WSADATA WSAData;
	WORD wVersionRequested;
	struct hostent	*hp;
    strcpy(address, addr);

	wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &WSAData) != 0)
    {
        ShowLastErr(true);
        qDebug() << "DLL not found!\n";
		return -1;
	}

    // TCP Open Socket
    if ((sendSock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        ShowLastErr(true);
        qDebug() << "Cannot create tcp socket\n";
        return -1;
    }

    // UDP Open Socket (if needed in future) ////////////////////
    /*if ((sendSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        qDebug() << "Cannot create udp socket\n";
        return -1;
    }*/

	// Initialize and set up the address structure
    memset((char *)&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_DEFAULT_PORT);
    if ((hp = gethostbyname(address)) == NULL)
	{
        ShowLastErr(true);
        qDebug() << "Unknown server address\n";
		return -1;
	}

	// Copy the server address
    memcpy((char *)&server.sin_addr, hp->h_addr, hp->h_length);

	
    // TCP Connecting to the server
    if (connect(sendSock, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        ShowLastErr(true);
        qDebug() << "Can't connect to server\n";
        return -1;
    }

    sendSockOpen = true;

    qDebug() << "Setup success";
	return 0;
}

int ClientSendSetupP2P(char* addr) {
    WSADATA WSAData;
    WORD wVersionRequested;
    struct hostent	*hp;
    strcpy(p2pAddress, addr);

    wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &WSAData) != 0) {
        ShowLastErr(true);
        qDebug() << "DLL not found";
        return -1;
    }

    // TCP Open Socket
    if ((p2pSendSock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        ShowLastErr(true);
        qDebug() << "Cannot create tcp socket";
        return -1;
    }

    // Initialize and set up the address structure
    memset((char *)&otherClient, 0, sizeof(struct sockaddr_in));
    otherClient.sin_family = AF_INET;
    otherClient.sin_port = htons(P2P_DEFAULT_PORT);
    if ((hp = gethostbyname(p2pAddress)) == NULL) {
        ShowLastErr(true);
        qDebug() << "Unknown server address";
        return -1;
    }


    // Copy the server address
    memcpy((char *)&otherClient.sin_addr, hp->h_addr, hp->h_length);

    qDebug () << "Attempting to accept request to ip " << p2pAddress << " on port " << P2P_DEFAULT_PORT;

    // TCP Connecting to the server
    if (connect(p2pSendSock, (struct sockaddr *)&otherClient, sizeof(otherClient)) == -1) {
        ShowLastErr(true);
        qDebug() << "Can't connect to server";
        return -1;
    }

    qDebug () << "Connected!";

    p2pSendSockOpen = true;

    return 0;
}

/*---------------------------------------------------------------------------------------
--	FUNCTION:   ClientSend
--
--
--	DATE:			April 7, 2016
--
--	REVISIONS:
--
--	DESIGNERS:		Micah Willems
--
--	PROGRAMMER:		Micah Willems
--
--  INTERFACE:      int ClientSend(HANDLE hFile)
--
--                      HANDLE hFile: a handle to the file to be sent
--
--  RETURNS:        Returns 0 on success, -1 on failure
--
--	NOTES:
--      This is the function that starts the thread to send a file to the server
---------------------------------------------------------------------------------------*/
int ClientSend(HANDLE hFile)
{
    HANDLE hThread;
    DWORD ThreadId;
    hSendOpen = true;

    if ((hThread = CreateThread(NULL, 0, ClientSendThread, (LPVOID)hFile, 0, &ThreadId)) == NULL)
    {
        ShowLastErr(false);
        qDebug() << "Create Send Thread failed";
        return -1;
    }
    return 0;
}

//Written by Carson, designed by Micah since it follows her other thread design
DWORD WINAPI ClientSendMicrophoneThread(LPVOID lpParameter) {
    hSendFile = (HANDLE) lpParameter;
    char *sendbuff = (char *)calloc(CLIENT_PACKET_SIZE + 1, sizeof(char));
    DWORD  dwBytesRead;
    int sentBytes = 0;

    while (isRecording) {
        microphoneBuffer->seek(0);
        dwBytesRead = microphoneBuffer->read(sendbuff, CLIENT_PACKET_SIZE);

        if (dwBytesRead > 0 && dwBytesRead < CLIENT_PACKET_SIZE - 5)
        {
            sendbuff[dwBytesRead] = 'm';
            sendbuff[dwBytesRead+1] = 'i';
            sendbuff[dwBytesRead+2] = 'l';
            sendbuff[dwBytesRead+3] = 'e';
            sendbuff[dwBytesRead+4] = 'd';

        }
        if (microphoneBuffer->size() > 0)
        {
            microphoneBuffer->seek(microphoneBuffer->size()-1);
        }

        sentBytes = send(p2pSendSock, sendbuff, CLIENT_PACKET_SIZE, 0);
#ifdef DEBUG_MODE
        qDebug() << "\nRecorded bytes:" << dwBytesRead;
        qDebug() << "Sent bytes:" << sentBytes;
#endif
    }

    sentBytes = send(p2pSendSock, sendbuff, CLIENT_PACKET_SIZE, 0);

    return TRUE;
}

//Written by Carson, Designed by Micah since it follows her other functions design
int ClientSendMicrophoneData() {
    HANDLE hThread;
    DWORD ThreadId;

    if ((hThread = CreateThread(NULL, 0, ClientSendMicrophoneThread, 0, 0, &ThreadId)) == NULL) {
        ShowLastErr(false);
        qDebug() << "Create ClientSendMicrophoneThread failed";
        return -1;
    }

    return 0;
}

/*---------------------------------------------------------------------------------------
--	FUNCTION:   ClientSendThread
--
--
--	DATE:			April 7, 2016
--
--	REVISIONS:
--
--	DESIGNERS:		Micah Willems
--
--	PROGRAMMER:		Micah Willems
--
--  INTERFACE:      DWORD WINAPI ClientSendThread(LPVOID lpParameter)
--
--                      LPVOID lpParameter: the handle to the file to be sent
--
--  RETURNS:        Returns TRUE on success, FALSE on failure
--
--	NOTES:
--      This is the threaded function that sends a file to the server. When the last
--      portion of the data is read, a delimeter is appended to indicate that it's the
--      end on the server's side.
---------------------------------------------------------------------------------------*/
DWORD WINAPI ClientSendThread(LPVOID lpParameter) {
    hSendFile = (HANDLE) lpParameter;
    char *sendbuff = (char *)calloc(CLIENT_PACKET_SIZE + 1, sizeof(char));
	DWORD  dwBytesRead;
    int sentBytes = 0;
	while (true) {
        if (ReadFile(hSendFile, sendbuff, CLIENT_PACKET_SIZE, &dwBytesRead, NULL) == FALSE)
		{
            ShowLastErr(false);
            qDebug() << "Couldn't read file";
            ClientCleanup();
            return FALSE;
        }

        if (dwBytesRead == 0) {
            ClientCleanup();
            return TRUE;
        }
        else if (dwBytesRead > 0 && dwBytesRead < (DWORD)CLIENT_PACKET_SIZE)
        {
            sendbuff[dwBytesRead]     = (int)'d';
            sendbuff[dwBytesRead + 1] = (int)'e';
            sendbuff[dwBytesRead + 2] = (int)'l';
            sendbuff[dwBytesRead + 3] = (int)'i';
            sendbuff[dwBytesRead + 4] = (int)'m';
#ifdef DEBUG_MODE
            qDebug() << "Delimeter attached";
#endif
        }

        // TCP Send
        sentBytes = send(sendSock, sendbuff, CLIENT_PACKET_SIZE, 0);
#ifdef DEBUG_MODE
        qDebug() << "Read bytes:" << dwBytesRead;
        qDebug() << "Sent bytes:" << sentBytes;
        qDebug() << "Total sent bytes:" << (totalbytessent += sentBytes);
#endif
    }
	return TRUE;
}

/*---------------------------------------------------------------------------------------
--	FUNCTION:   ClientCleanup
--
--
--	DATE:			March 30, 2016
--
--	REVISIONS:		April 10, 2016
--
--	DESIGNERS:		Micah Willems
--
--	PROGRAMMER:		Micah Willems
--
--  INTERFACE:      void ClientCleanup()
--
--  RETURNS:        void
--
--	NOTES:
--      This function checks every handle and socket flag on the client to see if anything
--      needs to be closed. It is used for disconnect buttons on the UI.
---------------------------------------------------------------------------------------*/
void ClientCleanup()
{
    if (sendSockOpen)
    {
        closesocket(sendSock);
        sendSockOpen = false;
    }
    if (listenSockOpen)
    {
        closesocket(listenSockOpen);
        listenSockOpen = false;
    }
    if (acceptSockOpen)
    {
        closesocket(acceptSock);
        acceptSockOpen = false;
    }
    if (hSendOpen)
    {
        closesocket(sendSock);
        hSendOpen = false;
    }
    if (hReceiveOpen)
    {
        CloseHandle(hReceiveFile);
        hReceiveOpen = false;
    }
    WSACleanup();
}

/*---------------------------------------------------------------------------------------
--	FUNCTION:   ShowLastErr
--
--
--	DATE:			January 19, 2016
--
--	REVISIONS:		February 13, 2016
--
--	DESIGNERS:		Micah Willems
--
--	PROGRAMMER:		Micah Willems
--
--  INTERFACE:      void ShowLastErr()
--
--  RETURNS:        void
--
--	NOTES:
--      This is a universal-purpose error reporting function for functions that can be
--		error checked using GetLastError. It uses FormatMessage to get an actual
--		human-readable, understandable string from the error ID and outputs it to the
--		Debug output console in the IDE.
---------------------------------------------------------------------------------------*/
void ShowLastErr(bool wsa)
{
    DWORD dlasterr;
    LPCTSTR errMsg = NULL;
    char errnum[100];
    if (wsa == true) {
        dlasterr = WSAGetLastError();
    }
    else {
        dlasterr = GetLastError();
    }
    sprintf_s(errnum, "Error number: %d", dlasterr);
    qDebug() << errnum;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL, dlasterr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errMsg, 0, NULL);
    qDebug() << QString::fromWCharArray(errMsg);
}
