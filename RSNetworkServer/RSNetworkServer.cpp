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


/**
*	Get error code
*	
*	@param resultCode - code of result of writting/readding operation
*	@return - error code 
*/
int GetErrorCode(int resultCode)
{
	int code = 0;
	switch (resultCode)
	{
		case ERR_OK:		code = 0; break;
		case ERR_IO:		code = (GetExtendedLastErr() == 0) ? GetDeviceLastErr() : GetExtendedLastErr(); break;
		case ERR_NERR:		code = (GetExtendedLastErr() == 0) ? GetDeviceLastErr() : GetExtendedLastErr(); break;
		case ERR_DEVERR:	code = (GetExtendedLastErr() == 0) ? GetDeviceLastErr() : GetExtendedLastErr(); break;
		default: break;
	}
	return code;
}

/**
*	Read string parametr value
*
*	@param port - device addres
*	@param paramName - parametr name
*	@param adrType - address length 8/11 bits
*	@param error - operation error code
*	@return - readed string value or NULL
*/
char* ReadStr(int port, char* paramName, int adrType, int *error)
{
	unsigned long size = 0;
	char buffer[512];
	int result = OwenIO(port, adrType, 1, paramName, buffer, &size);
	*error = GetErrorCode(result);
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

/**
*	Read int parametr value
*
*	@param value - returning readed value
*	@param port - device addres
*	@param paramName - parametr name
*	@param adrType - address length 8/11 bits
*	@param error - operation error code
*	@param unsign - unsigned type flag
*	@return - True if success, else - False
*/
bool ReadInt(int* value, int port, char* paramName, int adrType, int *error, bool unsign = false)
{
	int result = 0;
	if (unsign)
	{
		//unsigned value reading
		unsigned int temp = 0;
		result = ReadUInt(port, adrType, paramName, temp, -1);
		*error = GetErrorCode(result);
		if (result == ERR_OK)
		{
			*value = (int)temp;
			return true;
		}
	}
	else
	{
		//value reading
		result = ReadSInt(port, adrType, paramName, *value, -1);
		*error = GetErrorCode(result);
		if (result == ERR_OK)
		{
			return true;
		}
	}
	return false;
}

/**
*	Write int parametr value
*
*	@param value - writing int value
*	@param port - device addres
*	@param paramName - parametr name
*	@param adrType - address length 8/11 bits
*	@param error - operation error code
*	@param word - value word length flag
*	@return - True if success, else - false
*/
bool WriteInt(int value, int port, char* paramName, int adrType, int *error, bool word = false)
{
	int result = 0;
	if (word)
	{
		//writing word value
		result = WriteWord(port, adrType, paramName, value, -1);
		*error = GetErrorCode(result);
		if (result == ERR_OK)
		{
			return true;
		}
	}
	else
	{
		//writing byte value
		result = WriteByte(port, adrType, paramName, value, -1);
		*error = GetErrorCode(result);
		if (result == ERR_OK)
		{
			return true;
		}

	}
	return false;
}

/**
*	Read float parametr value
*
*	@param value - returning readed value
*	@param port - device addres
*	@param paramName - parametr name
*	@param adrType - address length 8/11 bits
*	@param error - operation error code
*	@param time - returned time value for complex data with time
*	@return - True if success, else - False
*/
bool ReadFloat(float *value, int port, char* paramName, int adrType, int *error, int *time = NULL)
{
	int result = 0;
	if (time == NULL)
	{
		result = ReadFloat24(port, adrType, paramName, *value, -1);
		*error = GetErrorCode(result);
		if (result == ERR_OK)
		{
			return true;
		}
	}
	else
	{
		result = ReadIEEE32(port, adrType, paramName, *value, *time, -1);
		*error = GetErrorCode(result);
		if(result == ERR_OK)
		{
			return true;
		}
	}
	return false;
}

/**
*	Write float parametr value
*
*	@param value - writing float value
*	@param port - device addres
*	@param paramName - parametr name
*	@param adrType - address length 8/11 bits
*	@param error - operation error code
*	@return - True if success, else - false
*/
bool WriteFloat(float value, int port, char* paramName, int adrType, int *error)
{
	int result = 0;
	result = WriteFloat24(port, adrType, paramName, value, -1);
	*error = GetErrorCode(result);
	if (result == ERR_OK)
	{
		return true;
	}
	return false;
}

/**
*	Close connection with RS-485 network
*/
void CloseConnection()
{
	ClosePort();
	devConnection = false;
}

//Search devices addreses

/**
*	Search devices addesses for devices collection
*
*	@return True if all devices addresses was found, else - False
*/
bool SearchDevices() 
{
	int code = 0;
	for (int i = 1; i < 256; i++)
	{
		//read device name
		char* devName = ReadStr(i, (char*)"dEv", ADRTYPE_8BIT, &code);
		if (devName != NULL && code == 0)
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
			if (devices[j]->address <= 0)
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
	CloseConnection();
	return -1;
}

/* END OF OWEN/RS-485 */

/* COMMON */

/**
*	Get param value from buffer
*
*	@params buffer - string existing list of parametrs
*	@params paramName - parametr name
*	@return param value or NULL
*/
char* GetParam(char* buffer, char* paramName)
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
*	- Success: status - 1, index - index of added device;
*	- Unsuccess: status - 0, error - error message;
*
*	@param request - Request string
*	@return - adding device responce string
*/
char* AddDeviceAction(char* request)
{
	//checking RS conection status 
	if(devConnection)
	{
		//error server already connected to RS-485 network
		return (char*)"{status:0;error:already_connected;}";
	}
	//get device name
	char* devName = GetParam(request, (char*)"device");
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
		devices[devCount]->name = newDevice.name;
		devices[devCount]->address = newDevice.address;
		devCount ++;
		//preparing response
		char* response = (char*)malloc((sizeof(char) * bufLen) + 1);
		sprintf_s(response, bufLen, "{status:1;index:%d}", (devCount-1));
		return response;
	}
	return (char*)"{status:0;error:adding_error;}";
}

/**
*	Remove devices from collection
*	Response:
*	- Success: status - 1;
*	- Unsuccess: status - 0, error - error message;
*
*	@return - removing devices response string
*/
char* RemoveDevicesAction()
{
	//checking RS conection status 
	if(devConnection)
	{
		//error server already connected to RS-485 network
		return (char*)"{status:0;error:already_connected;}";
	}
	//clear devices collection
	devices = (Device**)calloc(sizeof(Device), 0);
	devCount = 0;
	return (char*)"{status:1;}";
}

/**
*	Disconnect client from server
*	Response:
*	- Success:  status - 1;
*
*	@return - disconnect from server response string
*/
char* DisconnectAction()
{
	//close connection
	connectionStatus = false;
	return (char*)"{status:1;}";
}

/**
*	Command to die to server
*	Response:
*	- Success:  status - 1;
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
*	- Success: status - 1, port - number of connected COM-port
*	- Unsuccess: status - 0, error - error message
*
*	@return - autoconnection response string
*/
char* AutoRSConnectionAction()
{
	//devices collection checking
	if (devCount == 0)
	{
		//hasn't added devices
		return (char*)"{status:0;error:no_devices;}";
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
*	- Success:  status - 1;
*	- Unseccsses: status - 0, error - error message
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

/**
*	Read data from device
*	Request:
*	- dev_index - index of device in device collection (required)
*	- dev_channel - index of device channel (default - 0)
*	- param - name of reading parametr (required)
*	- type - type of reading parametr int/float/str (required)
*	- ieee - only for float type, flag of reading complex data with time
*	Response
*	- Success - status - 1, value - value of reading parametr
*	- Unsuccess - status - 0, error - error message
*	
*	@param request - request string
*	@return - response of reading data string
*/
char* ReadDataAction(char* request)
{
	//checking RS conection status 
	if(!devConnection)
	{
		//error server not connected to RS-485 network
		return (char*)"{status:0;error:no_connection_to_devices;}";
	}
	//checking device index
	if (strstr(request, "dev_index") == NULL)
	{
		return (char*)"{status:0;error:no_device;}";
	}
	//getting device index in collection
	char* tempIndex = GetParam(request, (char*)"dev_index");
	int devPort = atoi(tempIndex);
	//checking device channel
	int channel = 0;
	if (strstr(request, "dev_channel") != NULL)
	{
		tempIndex = GetParam(request, (char*)"dev_channel");
		channel = atoi(tempIndex);
	}
	//checking param name
	if (strstr(request, "param") == NULL)
	{
		return (char*)"{status:0;error:no_param_name;}";
	}
	//getting param name
	char* param = GetParam(request, (char*)"param");
	//checking param type
	if (strstr(request, "type") == NULL)
	{
		return (char*)"{status:0;error:no_param_type;}";
	}
	//getting param type
	char* type = GetParam(request, (char*)"type");
	int error = 0;
	char* response = (char*)calloc(sizeof(char), bufLen);
	//selecting reading function
	if (_strcmpi(type, (char*)"int") == 0) //int
	{
		int value;
		//checking value
		if (!ReadInt(&value, devices[devPort]->address + channel, param, ADRTYPE_8BIT, &error))
		{
			sprintf_s(response, bufLen, "{status:0;error:reading_error;code:%d;}", error);
			return response;
		}
		//preparing response
		sprintf_s(response, bufLen, "{status:1;value:%d;}", value);
		return response;
	}
	else if (_strcmpi(type, (char*)"float") == 0) //float
	{
		//value length
		bool ieee = false;
		if (strstr(request, "ieee") != NULL)
		{
			char* temp = GetParam(request, (char*)"ieee");
			//word length flag setting
			if (atoi(temp) == 1)
			{
				//Read IEEE 32
				float value;
				int time;
				//checking value
				if (!ReadFloat(&value, devices[devPort]->address + channel, param, ADRTYPE_8BIT, &error, &time))
				{
					sprintf_s(response, bufLen, "{status:0;error:reading_error;code:%d;}", error);
					return response;
				}
				//preparing response
				sprintf_s(response, bufLen, "{status:1;value:%f;time:%d;}", value, time);
				return response;
			}
		}
		//Read float 24
		float value;
		//checking value
		if (!ReadFloat(&value, devices[devPort]->address + channel, param, ADRTYPE_8BIT, &error))
		{
			sprintf_s(response, bufLen, "{status:0;error:reading_error;code:%d;}", error);
			return response;
		}
		//preparing response
		sprintf_s(response, bufLen, "{status:1;value:%f;}", value);
		return response;
	}
	else if (_strcmpi(type, (char*)"str") == 0) //string
	{
		char* value = ReadStr(devices[devPort]->address + channel, param, ADRTYPE_8BIT, &error);
		//checking value
		if (value == NULL)
		{
			sprintf_s(response, bufLen, "{status:0;error:reading_error;code:%d;}", error);
			return response;
		}
		//preparing response
		sprintf_s(response, bufLen, "{status:1;value:%s;}", value);
		return response;
	}
	else //unknown type
	{
		return (char*)"{status:0;error:unknown_type;}";
	}
}

/**
*	Write data to device
*	Request:
*	- drv_index - index of device in device collection (required)
*	- dev_cannel - index of device channel (default - 0)
*	- param - name of reading parametr (required)
*	- type - type of reading parametr int/float (required)
*	- value - writing to device value (required)
*	- double - flag or write 
*	Response
*	- Success - status - 1
*	- Unsuccess - status - 0, error - error message
*
*	@param request - request string
*	@return - response of reading data string
*/
char* WriteDataAction(char* request)
{
	//checking RS conection status 
	if(!devConnection)
	{
		//error server not connected to RS-485 network
		return (char*)"{status:0;error:no_connection_to_devices;}";
	}
	//checking existing writing value
	if (strstr(request, "value") == NULL)
	{
		return (char*)"{status:0;error:no_value;}";
	}
	char* tempValue = GetParam(request, (char*)"value");
	//checking device index
	if (strstr(request, "dev_index") == NULL)
	{
		return (char*)"{status:0;error:no_device;}";
	}
	//getting device index in collection
	char* tempIndex = GetParam(request, (char*)"dev_index");
	int devPort = atoi(tempIndex);
	//checking device channel
	int channel = 0;
	if (strstr(request, "dev_channel") != NULL)
	{
		tempIndex = GetParam(request, (char*)"dev_channel");
		channel = atoi(tempIndex);
	}
	//checking param name
	if (strstr(request, "param") == NULL)
	{
		return (char*)"{status:0;error:no_param_name;}";
	}
	//getting param name
	char* param = GetParam(request, (char*)"param");
	//checking param type
	if (strstr(request, "type") == NULL)
	{
		return (char*)"{status:0;error:no_param_type;}";
	}
	//getting param type
	char* type = GetParam(request, (char*)"type");
	int error = 0;
	char* response = (char*)calloc(sizeof(char), bufLen);
	//selecting reading function
	if (_strcmpi(type, (char*)"int") == 0) //int
	{
		//value length
		bool word = false;
		if (strstr(request, "word") != NULL)
		{
			char* temp = GetParam(request, (char*)"word");
			//word length flag setting
			if (atoi(temp) == 1)
			{
				word = true;
			}
		}
		int value = atoi(tempValue);
		//checking value
		if (!WriteInt(value, devices[devPort]->address + channel, param, ADRTYPE_8BIT, &error, word))
		{
			sprintf_s(response, bufLen, "{status:0;error:writting_error;code:%d;}", error);
			return response;
		}
		return (char*)"{status:1;}";
	}
	else if (_strcmpi(type, (char*)"float") == 0) //float
	{
		float value = atof(tempValue);
		//checking value
		if (!WriteFloat(value, devices[devPort]->address + channel, param, ADRTYPE_8BIT, &error))
		{
			sprintf_s(response, bufLen, "{status:0;error:writting_error;code:%d;}", error);
			return response;
		}
		return (char*)"{status:1;}";
	}
	else //unknown type
	{
		return (char*)"{status:0;error:unknown_type;}";
	}
}

// End of actions

/**
*	Parsing of request
*
*	@param buffer - Server receiving buffer
*	@param - Received data length
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
	char* action = GetParam(request, (char*)"action");
	if (action == NULL)
	{
		return (char*)"{status:0;error:no_action;}";
	}
	//action selection
	if (_strcmpi(action, "add_device") == 0)//add device
	{
		return AddDeviceAction(request);
	}
	if(_strcmpi(action, "remove_devices") == 0)//remove devices
	{
		return RemoveDevicesAction();
	}
	if (_strcmpi(action, "auto_connection") == 0)//autoconnect to RS-485 network
	{
		return AutoRSConnectionAction();
	}
	if (_strcmpi(action, "rs_disconnect") == 0)//disconnect from RS-485 network 
	{
		return RSDisconnect();
	}
	if (_strcmpi(action, "read_data") == 0)//read data from RS-485 device 
	{
		return ReadDataAction(request);
	}
	if (_strcmpi(action, "write_data") == 0)//write data to RS-485 device 
	{
		return WriteDataAction(request);
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
			if (result > 0)
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
			else if (result <= 0)
			{
				//error of data exchanging
				closesocket(clientSocket);
				connectionStatus = false;
				break;
			}
		} while (connectionStatus);
		//prepering for socket reconnection
		closesocket(clientSocket);
		if (devConnection)
		{
			CloseConnection();
		}
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
	serverPort = GetParam(buffer, (char*)"port");
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
	printf("Starting server...\n");
	//try load configuration
	if (!LoadConfiguration())
	{
		//error of loading configurations
		printf("Error! Confuration not found!\n");
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