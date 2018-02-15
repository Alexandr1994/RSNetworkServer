#include "stdafx.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "owen_io.h"
#include <stdio.h>
#include <windows.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")


/* OWEN FUNCTION */

//port
int port = 0;

//status output
bool statusOutput = true;

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
	if (statusOutput)
	{
		printf("Searching devices...\n");
	}
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
			if (statusOutput)
			{
				printf("All devices was found\n");
			}
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
	if (statusOutput)
	{
		printf("Connection closed\n");
	}
}

//open connection with RS-485 network
void OpenConnection()
{
	if (statusOutput)
	{
		printf("Try to connect...\n");
	}
	for (int i = 0; i < 8; i++) {
		int result = OpenPort(i, spd_9600, prty_NONE, databits_8, stopbit_1, RS485CONV_AUTO);
		if (result == ERR_OK) {
			if (statusOutput)
			{
				printf("Opened connection on port: %d\n", i);
			}
			hasConnection = true;
			port = i;
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
	param = (char*)(strchr(param, ':') + 1);
	int len = ((int)strchr(param, ';')) - (int)param;
	//init value
	char* value = (char*)calloc(sizeof(char), 1);
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

int main(int argc, _TCHAR* argv[])
{
	//try load configuration
	if (!LoadConfiguration())
	{
		if (statusOutput)
		{
			printf("Error loading of configuration!\n");
		}
		return -1;
	}

	//try to open connection with RS-485 netework
	OpenConnection();
	if (!hasConnection) {
		//Error: devices not found
		if (statusOutput)
		{
			printf("Error! Devices not found!\n");
		}
		return -1;
	}

	//close connection
	CloseConnection();
	//end of work
	return 0;
}