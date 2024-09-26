#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include <iostream>
#include <fstream>
#include <string>

#include "simlink.h"

using namespace std;

struct threadArgs {
    int stationNumber;
    int varType;
    int varIndex;
    SimLinkModel *model;
};

double convertBufferToDouble(unsigned char *buff)
{
	double returnVal;
	memcpy(&returnVal, buff, 8);

	return returnVal;
}

void sleep_ms(int milliseconds)
{
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

void getData(char *line, char *buf, char separator1, char separator2)
{
	int i=0, j=0;
	while (line[i] != separator1 && line[i] != '\0')
	{
		i++;
	}
	i++;

	while (line[i] != separator2 && line[i] != '\0')
	{
		buf[j] = line[i];
		i++;
		j++;
		buf[j] = '\0';
	}
}

int getStationNumber(char *line)
{
	char temp[5];
	int i = 0, j = 7;

	while (line[j] != '.')
	{
		temp[i] = line[j];
		i++;
		j++;
		temp[i] = '\0';
	}

	return(atoi(temp));
}

void getFunction(char *line, char *parameter)
{
	int i = 0, j = 0;

	while (line[j] != '.')
	{
		j++;
	}
	j++;

	while (line[j] != ' ' && line[j] != '=' && line[j] != '(')
	{
		parameter[i] = line[j];
		i++;
		j++;
		parameter[i] = '\0';
	}
}

void addStationPort(char *line, struct stationInfo *station_info)
{
	char type[100];
	uint16_t *dataPointer = NULL;
	getData(line, type, '(', ')');

	if(!strncmp(type, "digital_in", 10))
	{
		dataPointer = station_info->digitalInPorts;
	}

	else if(!strncmp(type, "digital_out", 11))
	{
		dataPointer = station_info->digitalOutPorts;
	}

	else if(!strncmp(type, "analog_in", 9))
	{
		dataPointer = station_info->analogInPorts;
	}

	else if(!strncmp(type, "analog_out", 10))
	{
		dataPointer = station_info->analogOutPorts;
	}
	else if(!strncmp(type, "generic_in", 9))
	{
		dataPointer = station_info->genericInPorts;
	}

	else if(!strncmp(type, "generic_out", 10))
	{
		dataPointer = station_info->genericOutPorts;
	}

	int i = 0;
	while (dataPointer[i] != 0)
	{
		i++;
	}

	char temp_buffer[100];
	getData(line, temp_buffer, '"', '"');
	dataPointer[i] = atoi(temp_buffer);
}

void parseConfigFile(SimLinkModel *model, char* file_path)
{
	string line;
	char line_str[1024];
	ifstream cfgfile(file_path);

	if (cfgfile.is_open())
	{
		while (getline(cfgfile, line))
		{
			strncpy(line_str, line.c_str(), 1024);
			if (line_str[0] != '#' && strlen(line_str) > 1)
			{
				if (!strncmp(line_str, "num_stations", 12))
				{
					char temp_buffer[10];
					getData(line_str, temp_buffer, '"', '"');
					model->numStations = atoi(temp_buffer);
					model->stationsData = (struct stationData *)malloc(model->numStations * sizeof(struct stationData));
					model->stationsInfo = (struct stationInfo *)malloc(model->numStations * sizeof(struct stationInfo));
				}
				else if (!strncmp(line_str, "comm_delay", 10))
				{
					char temp_buffer[10];
					getData(line_str, temp_buffer, '"', '"');
					model->commDelay = atoi(temp_buffer);
				}
				else if (!strncmp(line_str, "simulink", 8))
				{
					getData(line_str, model->simulinkIp, '"', '"');
				}

				else if (!strncmp(line_str, "station", 7))
				{
					int stationNumber = getStationNumber(line_str);
					char functionType[100];
					getFunction(line_str, functionType);

					if (!strncmp(functionType, "ip", 2))
					{
						getData(line_str, model->stationsInfo[stationNumber].ip, '"', '"');
					}
					else if (!strncmp(functionType, "add", 3))
					{
						addStationPort(line_str, &model->stationsInfo[stationNumber]);
					}
				}
			}
		}
		cfgfile.close();
	}
	else
	{
		cout << "Error trying to open file!" << endl;
	}
}

int createUDPServer(int port)
{
	int socket_fd;
	struct sockaddr_in server_addr;

	//Create TCP Socket
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd<0)
	{
		perror("Server: error creating stream socket");
		exit(1);
	}

	//Initialize Server Struct
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	//Bind socket
	if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Server: error binding socket");
		exit(1);
	}

	printf("Socket %d binded successfully on port %d!\n", socket_fd, port);

	return socket_fd;
}

void *sendSimulinkData(void *args)
{
	//getting arguments
	threadArgs *rcv_args = (threadArgs *)args;
	char* simulink_ip = rcv_args->model->simulinkIp;
	int stationNumber = rcv_args->stationNumber;
	int varType = rcv_args->varType;
	int varIndex = rcv_args->varIndex;
	SimLinkModel* model = rcv_args->model;

	int socket_fd, port;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int send_len;
	int16_t *analogPointer;
	bool *digitalPointer;
	DataPacket *genericPointer;

	//Create TCP Socket
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd<0)
	{
		perror("Server: error creating stream socket");
		exit(1);
	}

	//Figure out information about variable
	switch (varType)
	{
		case TYPE_ANALOGOUT:
			port = model->stationsInfo[stationNumber].analogOutPorts[varIndex];
			analogPointer = &model->stationsData[stationNumber].analogOut[varIndex];
			break;
		case TYPE_DIGITALOUT:
			port = model->stationsInfo[stationNumber].digitalOutPorts[varIndex];
			digitalPointer = &model->stationsData[stationNumber].digitalOut[varIndex];
			break;
		case TYPE_GENERICOUT:
			port = model->stationsInfo[stationNumber].genericOutPorts[varIndex];
			genericPointer = &model->stationsData[stationNumber].genericOut[varIndex];
			break;
	}

	//Initialize Server Structures
	server = gethostbyname(simulink_ip);
	if (server == NULL)
	{
		printf("Error locating host %s\n", simulink_ip);
		return 0;
	}

	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);

	while (1)
	{
		void* value;
		pthread_mutex_lock(&model->lock);
		if (varType == TYPE_GENERICOUT)
		{
			value = malloc(genericPointer->count * genericPointer->itemSize);
			memcpy(value, genericPointer->data, genericPointer->count * genericPointer->itemSize);
		}
		else if (varType == TYPE_ANALOGOUT)
		{
			value = malloc(sizeof(int16_t));
			memcpy(value, analogPointer, sizeof(int16_t));
		}
		else if (varType == TYPE_DIGITALOUT)
		{
			value = malloc(sizeof(int16_t));
			memcpy(value, digitalPointer, sizeof(int16_t));
		}

		//sprintf(value, "%d", *digitalPointer) : sprintf(value, "%d", *analogPointer);
		pthread_mutex_unlock(&model->lock);

		/*
		//DEBUG
		char varType_str[50];
		(varType == TYPE_ANALOGOUT) ? strncpy(varType_str, "TYPE_ANALOGOUT", 50) : strncpy(varType_str, "TYPE_DIGITALOUT", 50);
		printf("Sending data type %s, station %d, index %d, value: %d\n", varType_str, stationNumber, varIndex, value);
		*/
		const char* value_bytes = (const char*)value;

		unsigned long long size = (varType == TYPE_GENERICOUT) ? genericPointer->count * genericPointer->itemSize : sizeof(int16_t);

		send_len = sendto(socket_fd, value_bytes, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (send_len < 0)
		{
			printf("Error sending data to simulink on socket %d\n", socket_fd);
		}

		sleep_ms(model->commDelay);
	}
}

void *receiveSimulinkData(void *args)
{
	//getting arguments
	threadArgs *rcv_args = (threadArgs *)args;
	int stationNumber = rcv_args->stationNumber;
	int varType = rcv_args->varType;
	int varIndex = rcv_args->varIndex;
	SimLinkModel* model = rcv_args->model;

	int socket_fd, port;
	const int BUFF_SIZE = 1024;
	int rcv_len;
	unsigned char rcv_buffer[BUFF_SIZE];
	socklen_t cli_len;
	struct sockaddr_in client;
	int16_t *analogPointer;
	bool *digitalPointer;
	DataPacket *genericPointer;

	cli_len = sizeof(client);

	switch (varType)
	{
		case TYPE_ANALOGIN:
			port = model->stationsInfo[stationNumber].analogInPorts[varIndex];
			analogPointer = &model->stationsData[stationNumber].analogIn[varIndex];
			break;
		case TYPE_DIGITALIN:
			port = model->stationsInfo[stationNumber].digitalInPorts[varIndex];
			digitalPointer = &model->stationsData[stationNumber].digitalIn[varIndex];
			break;
		case TYPE_GENERICIN:
			port = model->stationsInfo[stationNumber].genericInPorts[varIndex];
			genericPointer = &model->stationsData[stationNumber].genericIn[varIndex];
			break;
	}

	socket_fd = createUDPServer(port);

	while(1)
	{
		rcv_len = recvfrom(socket_fd, rcv_buffer, BUFF_SIZE, 0, (struct sockaddr *) &client, &cli_len);
		if (rcv_len < 0)
		{
			printf("Error receiving data on socket %d\n", socket_fd);
		}

		else if ( varType == TYPE_GENERICIN && rcv_len > genericPointer->maxSize)
		{
			printf("%d\t%d\n", rcv_len,genericPointer->maxSize);
			printf("Received data exceeds buffer size on socket %d\n", socket_fd);
		}

		else
		{

			double valueRcv;

			pthread_mutex_lock(&model->lock);
			switch (varType)
			{
			case TYPE_ANALOGIN:
				valueRcv = convertBufferToDouble(rcv_buffer);
				*analogPointer = (int16_t)valueRcv;
				break;
			case TYPE_DIGITALIN:
				valueRcv = convertBufferToDouble(rcv_buffer);
				*digitalPointer = (valueRcv > 0) ? true : false;
				break;
			case TYPE_GENERICIN:
				genericPointer->count = rcv_len / genericPointer->itemSize;
				memset(genericPointer->data, 0, genericPointer->maxSize);
				memcpy(genericPointer->data, rcv_buffer, rcv_len);
				break;
			default:
				break;
			}
			pthread_mutex_unlock(&model->lock);
		}

		sleep_ms(model->commDelay);
	}
}

void sendGenericData(int idx, SimLinkModel *model)
{
	int j = 0;
	while (model->stationsInfo[idx].genericOutPorts[j] != 0)
	{
		struct threadArgs *args = new struct threadArgs();
		args->stationNumber = idx;
		args->varType = TYPE_GENERICOUT;
		args->varIndex = j;
		args->model = model;

		pthread_t sendingThread;
		pthread_create(&sendingThread, NULL, sendSimulinkData, args);
		j++;
	}
}

void sendAnalogData(int idx, SimLinkModel *model)
{
	int j = 0;
	while (model->stationsInfo[idx].analogOutPorts[j] != 0)
	{
		struct threadArgs *args = new struct threadArgs();
		args->stationNumber = idx;
		args->varType = TYPE_ANALOGOUT;
		args->varIndex = j;
		args->model = model;

		pthread_t sendingThread;
		pthread_create(&sendingThread, NULL, sendSimulinkData, args);
		j++;
	}
}

void sendDigitalData(int idx, SimLinkModel *model)
{
	int j = 0;
	while (model->stationsInfo[idx].digitalOutPorts[j] != 0)
	{
		struct threadArgs *args = new struct threadArgs();
		args->stationNumber = idx;
		args->varType = TYPE_DIGITALOUT;
		args->varIndex = j;
		args->model = model;

		pthread_t sendingThread;
		pthread_create(&sendingThread, NULL, sendSimulinkData, args);
		j++;
	}
}

void receiveAnalogData(int idx, SimLinkModel *model)
{
	int j = 0;
	while (model->stationsInfo[idx].analogInPorts[j] != 0)
	{
		struct threadArgs *args = new struct threadArgs();
		args->stationNumber = idx;
		args->varType = TYPE_ANALOGIN;
		args->varIndex = j;
		args->model = model;

		pthread_t receivingThread;
		pthread_create(&receivingThread, NULL, receiveSimulinkData, args);
		j++;
	}
}

void receiveDigitalData(int idx, SimLinkModel *model)
{
	int j = 0;
	while (model->stationsInfo[idx].digitalInPorts[j] != 0)
	{
		struct threadArgs *args = new struct threadArgs();
		args->stationNumber = idx;
		args->varType = TYPE_DIGITALIN;
		args->varIndex = j;
		args->model = model;

		pthread_t receivingThread;
		pthread_create(&receivingThread, NULL, receiveSimulinkData, args);
		j++;
	}
}

void receiveGenericData(int idx, SimLinkModel *model)
{
	int j = 0;
	while (model->stationsInfo[idx].genericInPorts[j] != 0)
	{
		struct threadArgs *args = new struct threadArgs();
		args->stationNumber = idx;
		args->varType = TYPE_GENERICIN;
		args->varIndex = j;
		args->model = model;

		pthread_t receivingThread;
		pthread_create(&receivingThread, NULL, receiveSimulinkData, args);
		j++;
	}
}

void exchangeDataWithSimulink(SimLinkModel *model)
{
	for (int i = 0; i < model->numStations; i++)
	{
		//sending analog data
		sendAnalogData(i, model);

		//receiving analog data
		receiveAnalogData(i, model);

		//sending digital data
		sendDigitalData(i, model);

		//receiving digital data
		receiveDigitalData(i, model);

		//sending generic data
		sendGenericData(i, model);

		//receiving generic data
		receiveGenericData(i, model);
	}
}

void loadModel(SimLinkModel** model, char* file_path)
{
	*model = (SimLinkModel *)malloc(sizeof(SimLinkModel));
	parseConfigFile(*model, file_path);
}

void displayInfo(SimLinkModel *model)
{
	for (int i = 0; i < model->numStations; i++)
	{
		printf("\nStation %d:\n", i);
		printf("ip: %s\n", model->stationsInfo[i].ip);

		int j = 0;
		while (model->stationsInfo[i].analogInPorts[j] != 0)// && j <= 4)
		{
			printf("AnalogIn %d: %d\n", j, model->stationsInfo[i].analogInPorts[j]);
			j++;
		}

		j = 0;
		while (model->stationsInfo[i].analogOutPorts[j] != 0)// && j <= 4)
		{
			printf("AnalogOut %d: %d\n", j, model->stationsInfo[i].analogOutPorts[j]);
			j++;
		}

		j = 0;
		while (model->stationsInfo[i].digitalInPorts[j] != 0)// && j <= 4)
		{
			printf("DigitalIn %d: %d\n", j, model->stationsInfo[i].digitalInPorts[j]);
			j++;
		}

		j = 0;
		while (model->stationsInfo[i].digitalOutPorts[j] != 0)// && j <= 4)
		{
			printf("DigitalOut %d: %d\n", j, model->stationsInfo[i].digitalOutPorts[j]);
			j++;
		}

		j = 0;
		while (model->stationsInfo[i].genericInPorts[j] != 0)// && j <= 4)
		{
			printf("GenericIn %d: %d\n", j, model->stationsInfo[i].genericInPorts[j]);
			j++;
		}

		j = 0;
		while (model->stationsInfo[i].genericOutPorts[j] != 0)// && j <= 4)
		{
			printf("GenericOut %d: %d\n", j, model->stationsInfo[i].genericOutPorts[j]);
			j++;
		}
	}
}
