#include <stdlib.h>
#include <pthread.h>

#define TYPE_ANALOGIN		0
#define TYPE_ANALOGOUT		1
#define TYPE_DIGITALIN		2
#define TYPE_DIGITALOUT		3
#define TYPE_GENERICIN		4
#define TYPE_GENERICOUT		5

#define GENERIC_BUF_SIZE	8
#define ANALOG_BUF_SIZE		8
#define DIGITAL_BUF_SIZE	16

struct dataPacket
{
	void* data;
	uint16_t count;
	uint16_t itemSize;
	uint16_t maxSize;
} typedef DataPacket;

struct stationInfo
{
	char ip[100];
	uint16_t analogInPorts[ANALOG_BUF_SIZE];
	uint16_t analogOutPorts[ANALOG_BUF_SIZE];
	uint16_t digitalInPorts[DIGITAL_BUF_SIZE];
	uint16_t digitalOutPorts[DIGITAL_BUF_SIZE];
	uint16_t genericInPorts[ANALOG_BUF_SIZE];
	uint16_t genericOutPorts[ANALOG_BUF_SIZE];
} typedef StationInfo;

struct stationData
{
	int16_t analogIn[ANALOG_BUF_SIZE];
	int16_t analogOut[ANALOG_BUF_SIZE];
	bool digitalIn[DIGITAL_BUF_SIZE];
	bool digitalOut[DIGITAL_BUF_SIZE];
	struct dataPacket genericIn[ANALOG_BUF_SIZE];
	struct dataPacket genericOut[ANALOG_BUF_SIZE];
} typedef StationData;

struct simLinkModel
{
	char simulinkIp[100];
	uint8_t numStations;
	uint16_t commDelay;
	struct stationInfo* stationsInfo;
	struct stationData* stationsData;
	pthread_mutex_t lock;
} typedef SimLinkModel;

class SimLink
{

public:
	SimLink(SimLinkModel* model);

	~SimLink();

	/**
	 * @brief Print stations' info
	 * 
	 */
	void displayInfo();

	/**
	 * @brief Parse the interface.cfg file looking for the IP address of the Simulink app
	 * and for each station information
	 * 
	 * @param file_path 
	 */
	void loadModel(char* file_path);

	/**
	 * @brief Main function responsible to exchange data with the simulink application
	 * 
	 */
	void exchangeDataWithSimulink();

private:

	SimLinkModel *model;

	void sendGenericData(int idx);

	void sendAnalogData(int idx);

	void sendDigitalData(int idx);

	void receiveAnalogData(int idx);

	void receiveDigitalData(int idx);

	void receiveGenericData(int idx);

	void parseConfigFile(char* file_path);

};