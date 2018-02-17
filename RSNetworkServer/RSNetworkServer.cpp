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
typedef struct
{
	DWORD address;
	char* name;
} Device;

//devices count
int devCount = 0;

//devices collection
Device** devices = (Device**)calloc(sizeof(Device), 0);

//connection status true - 
bool devConnection = false;

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
int ReadInt(int port, char* paramName, int adrType, bool unsign = false)
{
	int result = 0;
	if (unsign)
	{
		unsigned int value = 0;
		result = ReadUInt(port, adrType, paramName, value, -1);
		if (result == ERR_OK)
		{
			return value;
		}
	}
	else
	{
		int value = 0;
		result = ReadSInt(port, adrType, paramName, value, -1);
		if (result == ERR_OK)
		{
			return value;
		}
	}
	return NULL;
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
double ReadFloat(int port, char* paramName, int adrType)
{
	float value = 0;
	int result = 0;
	result = ReadFloat24(port, adrType, paramName, value, -1);
	if (result == ERR_OK)
	{
		return value;
	}
	return NULL;
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

//close connection with RS-485 network
void CloseConnection()
{
	ClosePort();
	devConnection = false;
}

//Search devices addreses
bool SearchDevices() {
	for (int i = 1; i < 256; i++)
	{
		//read device name
		char* devName = ReadStr(i, (char*)"dEv", ADRTYPE_8BIT);
		if (devName != NULL)
		{
			for (int j = 0; j < devCount; j ++)
			{
				if (devices[j]->address == 0)
				{
					if (strcmp(devices[j]->name, devName) == 0)
					{
						devices[j]->address = i;
					}
				}
			}
		}
		//all devices searched cheacking
		bool searchedFlag = true;
		for (int j = 0; j < devCount; j++)
		{
			if (devices[j]->address == 0)
			{
				searchedFlag = false;
			}
		}
		if (searchedFlag)
		{
			return true;
		}
	}
	//some devices not found
	return false;
}

/**
*	Autoconnection with RS-485 network
*	
*	@return - number of connected COM-port or code of error
*/
int OpenAutoRSConnection()
{
	for (int i = 0; i < 8; i++) 
	{
		int result = OpenPort(i, spd_9600, prty_NONE, databits_8, stopbit_1, RS485CONV_AUTO);
		if (result == ERR_OK) 
		{
			devConnection = true;
			rsPort = i;
			//Searching devices on port
			if (SearchDevices())
			{
				//all devices was found
				return i;
			}
			else
			{
				//some devices not found - error
				CloseConnection();
				return -2;
			}
		}
	}
	//devices port not found
	devConnection = false;
	return -1;
}

/* END OF OWEN/RS-485 */

/* COMMON */

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

/* END OF COMMON */

/* SERVER */

//buffer length
const int bufLen = 512;

//server port
LPCSTR serverPort = "";

//server working flag
bool serverWork = true;

//connection with socket flag
bool connectionStatus = false;

// actions

/**
*	Add device action
*	Request:
*	- device - name of device (required)	
*	Response:
*	- Success: code - 1, index - index of added device;
*	- Unsuccess: code - 0, error - error message;
*
*	@param request - Request string
*	@return - adding device responce string
*/
char* AddDeviceAction(char* request)
{
	//get device name
	char* devName = getParam(request, (char*)"device");
	if (devName == NULL)
	{
		//error! device name is not listed
		return (char*)"{status:0;error:no_device;}";
	}
	//prepare adding device
	Device newDevice;
	newDevice.address = 0;
	newDevice.name = devName;
	*(devices + devCount) = (Device*)(malloc(sizeof(newDevice)));
	if (devices != NULL)
	{
		//adding device
		*((*devices) + devCount) = newDevice;
		devCount ++;
		//preparing response
		char* response = (char*)malloc((sizeof(char) * bufLen) + 1);
		sprintf_s(response, bufLen, "{status:1;index:%d}", (devCount-1));
		return response;
	}
	return (char*)"{status:0;error:adding_error;}";
}

/**
*	Disconnect client from server
*	Response:
*	- Success:  code - 1;
*
*	@return - cisconnect from server response string
*/
char* DisconnectAction()
{
	//clear devices collection
	devices = (Device**)calloc(sizeof(Device), 0);
	devCount = 0;
	//close connection
	connectionStatus = false;
	return (char*)"{status:1;}";
}

/**
*	Command to die to server
*	Response:
*	- Success:  code - 1;
*
*	@return - die server response string
*/
char* DieAction()
{
	//close connection
	connectionStatus = false;
	serverWork = false;
	return (char*)"{status:1;}";
}

/**
*	Autoconnection to devices
*	Response:
*	- Success: code - 1, port - number of connected COM-port
*	- Unsuccess: code - 0, error - error message
*
*	@return - autoconnection response string
*/
char* AutoRSConnectionAction()
{
	if (devCount == 0)
	{
		//hasn't added devices
		return (char*)"{status:0;error:port_not_found;}";

	}
	int result = OpenAutoRSConnection();
	//error checking
	if (result == -1)
	{
		//not found COM-port
		return (char*)"{status:0;error:port_not_found;}";
	}
	if (result == -2)
	{
		//not found some device
		return (char*)"{status:0;error:device_not_found;}";
	}
	//prepare response
	char* responce = (char*)calloc(sizeof(char), bufLen + 1);
	sprintf_s(responce, bufLen, "{status:1;port:%d;}", result);
	return responce;
}

/**
*	Disconnect from RS-485 network
*	Response:
*	- Success:  code - 1;
*	- Unseccsses: code - 0, error - error message
*
*	@return - RS disconnect response string
*/
char* RSDisconnect()
{
	if (devConnection)
	{
		CloseConnection();
		return (char*)"{status:1;}";
	}
	return (char*)"{status:0;error:was_not_connected;}";
}

char* WriteDataAction(char* request)
{
	//todo: reading from device data action
	return (char*)"{status:1;}";
}

char* WriteDataAction(char* request)
{
	//todo: writing to device data action
	return (char*)"{status:1;}";
}

// End of actions

/**
*	Parsing of request
*
*	@param buffer - Server receiving buffer
*	@param - Received data length
*
*	@return - Clean request string
*/
char* ParseRequest(char* buffer, int len)
{
	char* request = (char*)calloc(sizeof(char), len + 1);
	if (request == NULL)
	{
		return NULL;
	}
	else
	{
		for (int i = 0; i < len; i++)
		{
			*(request + i) = *(buffer + i);
		}
	}
	return request;
}

/**
*	Construct server response
*
*	@param buffer - Request string
*	@param len - Request string length
*	@return - Response string 
*/
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
	//action
	char* action = getParam(request, (char*)"action");
	if (action == NULL)
	{
		return (char*)"{status:0;error:no_action;}";
	}
	//action selection
	if (_strcmpi(action, "add_device") == 0)//add device
	{
		return AddDeviceAction(request);
	}
	if (_strcmpi(action, "auto_connection") == 0)//autoconnect to RS-485 network
	{
		return AutoRSConnectionAction();
	}
	if (_strcmpi(action, "rs_disconnect") == 0)//disconnect from RS-485 network 
	{
		return RSDisconnect();
	}

	if (_strcmpi(action, "disconnect") == 0)//disconnect from server 
	{
		return DisconnectAction();
	}
	if (_strcmpi(action, "die") == 0)//die server 
	{
		return DieAction();
	}
	return (char*)"{status:0;error:unknow_action;}";
}

/**
*	Server main process function
*
*	@return - Server status code
*/
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
	if (bind(listenSocket, resAddr->ai_addr, (int)resAddr->ai_addrlen) == SOCKET_ERROR)
	{
		//binfing error
		freeaddrinfo(resAddr);
		WSACleanup();
		return -2;
	}
	freeaddrinfo(resAddr);
	//server loop
	do
	{
		//listening
		if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			//listening error
			closesocket(clientSocket);
			WSACleanup();
			return -2;
		}
		//accepting client socket
		clientSocket = accept(listenSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET)
		{
			//error of accepting client socket
			continue;
		}
		connectionStatus = true;
		//data exchanging loop
		char buffer[bufLen];
		int result;
		do
		{
			result = recv(clientSocket, buffer, bufLen, 0);
			if (result >= 0)
			{
				char* response = ContructResponse(buffer, result);
				if (response == NULL)
				{
					response = (char*)"";
				}
				result = send(clientSocket, response, strlen(response), 0);
				if (result == SOCKET_ERROR)
				{
					//error of data exchanging
					closesocket(clientSocket);
					connectionStatus = false;
					break;
				}
			}
			else if (result < 0)
			{
				//error of data exchanging
				closesocket(clientSocket);
				connectionStatus = false;
				break;
			}
		} while (connectionStatus);
		closesocket(clientSocket);
	} while (serverWork);
	
	//shutdown server
	shutdown(clientSocket, SD_SEND); 
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}

/* END OF SERVER */

/* CONFIGURATION */

/**
*	Load app configuration from config-file
*
*	@return - True - if configuration loaded successful, else - False 
*/
bool LoadConfiguration()
{
	//open config file
	FILE* config;
	if (fopen_s(&config, "config.scnf", "r") != 0)
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
		counter++;
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
	try
	{
		FreeConsole();
	}
	catch (int error)
	{
		;
	}
	//server
	return ServerProcess();
}