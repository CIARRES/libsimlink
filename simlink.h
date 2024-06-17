#include <stdlib.h>
#include <pthread.h>

#define TYPE_ANALOGIN		0
#define TYPE_ANALOGOUT		1
#define TYPE_DIGITALIN		2
#define TYPE_DIGITALOUT		3

#define ANALOG_BUF_SIZE		8
#define DIGITAL_BUF_SIZE	16

#define PLC_STATIONS_PORT	6668

struct stationInfo
{
	char ip[100];
	uint16_t analogInPorts[ANALOG_BUF_SIZE];
	uint16_t analogOutPorts[ANALOG_BUF_SIZE];
	uint16_t digitalInPorts[DIGITAL_BUF_SIZE];
	uint16_t digitalOutPorts[DIGITAL_BUF_SIZE];
};
typedef struct stationInfo StationInfo;

struct stationData
{
	int16_t analogIn[ANALOG_BUF_SIZE];
	int16_t analogOut[ANALOG_BUF_SIZE];
	bool digitalIn[DIGITAL_BUF_SIZE];
	bool digitalOut[DIGITAL_BUF_SIZE];
};
typedef struct stationData StationData;

struct simLinkModel
{
	char simulinkIp[100];
	uint8_t numStations;
	uint16_t commDelay;
	struct stationInfo* stationsInfo;
	struct stationData* stationsData;
	pthread_mutex_t lock;
};

typedef struct simLinkModel SimLinkModel;

//-----------------------------------------------------------------------------
// Print stations' info
//-----------------------------------------------------------------------------
void displayInfo(simLinkModel *model);

//-----------------------------------------------------------------------------
// Parse the interface.cfg file looking for the IP address of the Simulink app
// and for each station information
//-----------------------------------------------------------------------------
void loadModel(simLinkModel** model, char* file_path);

//-----------------------------------------------------------------------------
// Main function responsible to exchange data with the simulink application
//-----------------------------------------------------------------------------
void exchangeDataWithSimulink(simLinkModel *model);