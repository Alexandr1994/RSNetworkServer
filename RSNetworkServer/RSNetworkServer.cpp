#include "stdafx.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "owen_io.h"
#include <windows.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")


/* RS-485/OWEN */

//port
int rsPort = 0;

//device
typedef struct {
	int address;
	char* name;
} device;

//devices collection
device* devices;

//connection status true - 
bool hasConnection = false;

//addresses of devices
DWORD counterAddress = 0;
DWORD inputModuleAddress = 0;
DWORD outputModuleAddress = 0;

//devices names
char* counterDevName = (char*)"SI30";
char* inputDevName = (char*)"MB110-8C";
char* outputDevName = (char*)"MU110-8I";

//read string value of param with name - paramName from device on addres - port
char* ReadStr(int port, char* paramName, int adrType)
{
	unsigned long size = 0;
	char buffer[500];
	int result = OwenIO(port, adrType, 1, paramName, buffer, &size);
	if (result == ERR_OK)
	{
		char* readedStr;
		readedStr = (char*)calloc(sizeof(char), size + 1);
		for (int i = 0; i < size; i++)
		{
			readedStr[(int)size - (i + 1)] = buffer[i];
		}
		return readedStr;
	}
	return NULL;
}

//read int/unsigned int value of param with name - paramName from device on addres - port
bool ReadInt(int* value, int port, char* paramName, int adrType, bool unsign = false)
{
	int result = 0;
	if (unsign)
	{
		unsigned int temp = 0;
		result = ReadUInt(port, adrType, paramName, temp, -1);
		if (result == ERR_OK)
		{
			return true;
		}
		*value = (int)temp;
	}
	else
	{
		result = ReadSInt(port, adrType, paramName, *value, -1);
		if (result == ERR_OK)
		{
			return true;
		}
	}
	return false;
}

//write one/teo bype int value to param with name - paramName from device on addres - port
bool WriteInt(int value, int port, char* paramName, int adrType, bool word = false)
{
	int result = 0;
	if (word)
	{
		result = WriteWord(port, adrType, paramName, value, -1);
		if (result == ERR_OK)
		{
			return true;
		}
	}
	else
	{
		result = WriteByte(port, adrType, paramName, value, -1);
		if (result == ERR_OK)
		{
			return true;
		}
	}
	return false;
}

//read float value of param with name - paramName from device on addres - port
bool ReadFloat(float* value, int port, char* paramName, int adrType)
{
	int result = 0;
	result = ReadFloat24(port, adrType, paramName, *value, -1);
	if (result == ERR_OK)
	{
		return true;
	}
	return false;
}

//write float value to param with name - paramName from device on addres - port
bool WriteFloat(float value, int port, char* paramName, int adrType)
{
	int result = 0;
	result = ReadFloat24(port, adrType, paramName, value, -1);
	if (result == ERR_OK)
	{
		return true;
	}
	return false;
}

//Search devices addreses
bool SearchDevices() {
	for (int i = 1; i < 256; i++)
	{
		//read device name
		char* devName = ReadStr(i, (char*)"dEv", ADRTYPE_8BIT);
		if (devName != NULL)
		{
			//counter
			if (counterAddress == 0)
			{
				if (strcmp(counterDevName, devName) == 0)
				{
					counterAddress = i;
				}
			}
			//inputDevice
			if (inputModuleAddress == 0)
			{
				if (strcmp(inputDevName, devName) == 0)
				{
					inputModuleAddress = i;
				}
			}
			//outputDevice
			if (outputModuleAddress == 0)
			{
				if (strcmp(outputDevName, devName) == 0)
				{
					outputModuleAddress = i;
				}
			}
		}
		//all devices searched
		if (inputModuleAddress > 0 && outputModuleAddress > 0 && counterAddress)
		{
			return true;
		}
	}
	//some devices not found
	return false;
}

//close connection with RS-485 network
void CloseConnection()
{
	ClosePort();
	hasConnection = false;
}

//open connection with RS-485 network
void OpenConnection()
{
	for (int i = 0; i < 8; i++) {
		int result = OpenPort(i, spd_9600, prty_NONE, databits_8, stopbit_1, RS485CONV_AUTO);
		if (result == ERR_OK) {
			hasConnection = true;
			rsPort = i;
			//Searching devices on port
			if (SearchDevices())
			{
				//all devices was found
				return;
			}
			else
			{
				//some devices not found - error
				CloseConnection();
				return;
			}
		}
	}
	hasConnection = false;
}

/* END OF OWEN FUNCTIONS */


/* CONFIGURATION */

const int bufLen = 512;

LPCSTR serverPort = "";

char* getParam(char* buffer, char* paramName)
{
	//check param existing
	if (strstr(buffer, paramName) == NULL)
	{
		return NULL;
	}
	//getting param value
	char* param = (char*)(strstr(buffer, paramName));
	if(param == NULL)
	{
		return NULL;
	}
	param = (char*)(strchr(param, ':') + 1);
	int len = ((int)strchr(param, ';')) - (int)param;
	//init value
	char* value = (char*)calloc(sizeof(char), len + 1);
	if(value == NULL)
	{
		return NULL;
	}
	memmove((void*)value, (void*)param, len);
	return value;
}

bool LoadConfiguration()
{
	//open config file
	FILE* config;
	if(fopen_s(&config, "config.scnf", "r") != 0)
	{
		return false;
	}
	//load config content
	char *buffer = (char*)calloc(sizeof(char), 1);;
	int counter = 0;
	while (!feof(config))
	{
		*(buffer + counter) = (char)calloc(sizeof(char), 1);
		fread((void*)(buffer + counter), 1, 1, config);
		counter ++;
	} 
	//getting port
	serverPort = getParam(buffer, (char*)"port");
	if (serverPort == NULL)
	{
		serverPort = "";
		fclose(config);
		return false;
	}
	
	fclose(config);
	return true;
}

/* END OF CONFIGURATION */

/* SERVER */

char* ParseRequest(char* buffer, int len)
{
	char* request = (char*)calloc(sizeof(char), len + 1);
	if(request == NULL)
	{
		return NULL;
	}
	else
	{
		for(int i = 0; i < len; i ++ )
		{
			*(request + i) = *(buffer + i);
		}
	}
	return request;
}

char* ContructResponse(char* buffer, int len)
{
	char* request = ParseRequest(buffer, len);
	if(request == NULL)
	{
		return NULL;
	}
	//request primary validation
	if(strstr(request, "{") == NULL || strstr(request, "}") == NULL)
	{
		return NULL;
	}
	request = (char*)(strchr(request, '{') + 1);
	int tempLen = ((int)strchr(request, '}')) - ((int)request);
	char *temp = (char*)calloc(sizeof(char), tempLen + 1);
	memmove((void*)temp, (void*)request, tempLen);

	return "";
}


int ServerProcess()
{
	//start server
	//WSA preparing
	WSADATA wsaData;
	if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
	{
		//error of starting server
		return -2;
	}
	//Addresses preparing
	struct addrinfo addrInfo, *resAddr;
	ZeroMemory(&addrInfo, sizeof(addrInfo));
	addrInfo.ai_family = AF_INET;
	addrInfo.ai_socktype = SOCK_STREAM;
	addrInfo.ai_protocol =IPPROTO_TCP;
	addrInfo.ai_flags = AI_PASSIVE;
	if(getaddrinfo(NULL, serverPort, &addrInfo, &resAddr) != 0)
	{
		//error of getting adres info
		WSACleanup();
		return -2;
	}
	//init socets
	SOCKET listenSocket = INVALID_SOCKET;
	SOCKET clientSocket = INVALID_SOCKET;
	//listen socet
	listenSocket = socket(resAddr->ai_family, resAddr->ai_socktype, resAddr->ai_protocol);
	if(listenSocket == INVALID_SOCKET)
	{
		//error of getting listen socket
		WSACleanup();
		return -2;
	}
	//binding
	if(bind(listenSocket, resAddr->ai_addr, (int)resAddr->ai_addrlen) == SOCKET_ERROR)
	{
		//binfing error
		freeaddrinfo(resAddr);
		WSACleanup();
		return -2;
	}
	freeaddrinfo(resAddr);
	//listening
	if(listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		//listening error
		closesocket(clientSocket);
		WSACleanup();
		return -2;
	}
	//accepting client socket
	clientSocket = accept(listenSocket, NULL, NULL);
	if(clientSocket == INVALID_SOCKET) 
	{
		//error of accepting client socket
		closesocket(clientSocket);
		WSACleanup();
		return -2;
	}
	//data exchanging
	char buffer[bufLen];
	int result, tempResult;
	do
	{
		result = recv(clientSocket, buffer, bufLen, 0);
		if(result >= 0)
		{

			char* response = ContructResponse(buffer, result);
			if(response == NULL)
			{
				response = "";
			}


			result = send(clientSocket, response, strlen(response), 0);
			if(result == SOCKET_ERROR)
			{
				//error of data exchanging
				closesocket(clientSocket);
				WSACleanup();
				return -2;
			}
		}
		else if(result < 0)
		{
			//error of data exchanging
			closesocket(clientSocket);
			WSACleanup();
			return -1;
		}
	}
	while(true);
	//shutdown server
	shutdown(clientSocket, SD_SEND); 
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}

/* END OF SERVER */

int main(int argc, _TCHAR* argv[])
{
	//try load configuration
	if (!LoadConfiguration())
	{
		//error of loading configurations
		return -1;
	}
	//free std io handles and console
	CloseHandle(GetStdHandle(STD_INPUT_HANDLE));	//input
	CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));	//output
	CloseHandle(GetStdHandle(STD_ERROR_HANDLE));	//error
	//server
	return ServerProcess();

	//try to open connection with RS-485 netework
	OpenConnection();
	if (!hasConnection) {
		//Error: devices not found
		return -3;
	}

	//close connection
	CloseConnection();
	//end of work
	return 0;
}