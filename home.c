///////////////////////////////////
//Author:       Wai Leong Cheong
//Maturity:     Draft / Sketch
//Description:  "home" application which shall
//				- Establish secure TSL connection with MQTT protocol
//				- Register account for newly connected SBC
//				- Update sensor value in local array


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "mosquitto.h"
#include "home.h"


/* Mosquitto Callbacks */
void MessageCallBack(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
void ConnectCallBack(struct mosquitto *mosq, void *userdata, int result);
void DisconnectCallBack(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);


/*Local function prototype */
int PayloadParser (SBC_t *SbcObject , void* payload, int MsgLen);
void GetSysTime(struct tm *SbcTime);


/* Global Declaration */
myDB_t myDB[2000];
static int run = 1;


int main(int argc, char *argv[])
{
	
       char *host = "localhost";		//MQTT Broker running on the same machine
       int port = 8883;				//Default port for secure TLS
       int keepalive = 60;			//Interval of 60s to ensure connection health
       bool clean_session = true;

	struct mosquitto *MosquittoInstance = NULL;
	 int rc = 0;  //MOSQ_ERR_SUCCESS
	
	mosquitto_lib_init();

       MosquittoInstance = mosquitto_new(NULL, clean_session, NULL);		/*Create mosquitto client instance */
       if(!MosquittoInstance){
                printf("Failed to create mosquitto client instance.\n");
                return 1;
        	}


	// Set up security connection
	//mosquitto_tls_opts_set(mosq, 1, "tlsv1", NULL);
	//mosquitto_tls_set(mosq, "../ssl/test-root-ca.crt", "../ssl/certs", "../ssl/client.crt", "../ssl/client.key", NULL);
	mosquitto_tls_set(MosquittoInstance, \
	"/etc/mosquitto/ca_certificates/ca.crt", \	//To provide certificate authority cert
	"/etc/mosquitto/certs", \
	"/etc/mosquitto/certs/server.crt", \		//To provide server cert
	"/etc/mosquitto/certs/server.key", \		//To provide server private key
	NULL);	

	// Set up callback function for scenario
	mosquitto_subscribe_callback_set(MosquittoInstance, MessageCallBack);
	mosquitto_connect_callback_set(MosquittoInstance, ConnectCallBack);
	mosquitto_disconnect_callback_set(MosquittoInstance, DisconnectCallBack);


	// MQTT Connect
	rc = mosquitto_connect(MosquittoInstance, host, port, keepalive);
	if (rc != 0)
		{
			printf ("Failed to connect. Reason: %d", &rc);
			return 1;
		}


	// Wait for incoming message
	while(run){
			rc = mosquitto_loop(MosquittoInstance, -1, 1);
			
			if(run && rc){
				printf("connection error!\n");
				mosquitto_reconnect(MosquittoInstance);	//if connection dropped due to whatever reason, reconnect it
			}
		}

	mosquitto_destroy(MosquittoInstance);
	mosquitto_lib_cleanup();
	
	return 0;
}


 /////////////////////////////////////////////////////////////////////////////////////////////////////
//Function:		MessageCallBack
//Description:		Callback function for MQTT incoming message. 

void MessageCallBack(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	SBC_t SbcInfo;
	bool FirstConnect=TRUE;
	int Record = 0;
	
	if(message->payloadlen) {
		printf("%s :: %d\n", message->topic, message->payloadlen);
	} else {
		printf("%s (null)\n", message->topic);
	}

	//Decode payload for ProductID, Irradiance, Current values
	PayloadParser(&SbcInfo, message->payload, message->payloadlen);

	//Check SBC has already registered a valid record
	for (Record=0 ; Record < 2000 ; Record++)
		{
			if (myDB[Record].valid == 1) {								// Check whether is valid record
				if (strcmp(myDB[Record].ProdId, SbcInfo.ProdId) == 0) {		//Found instance in table -> registered SBC
					FirstConnect = FALSE;
					break;	//Found a valid record, stop checking
				}
			}
			else {
				break;		// If no more valid record, stop checking
			}
		}

	if (FirstConnect == TRUE)	// First connect, registering an account
		{
			myDB[Record].valid = 1;
			strcpy(myDB[Record].ProdId, SbcInfo.ProdId);
			GetSysTime(&myDB[Record].FirstConnectTime); 
		}
	
	//Update sensor value
	strcpy( myDB[Record].Current, SbcInfo.Current );
	strcpy( myDB[Record].Irradiance, SbcInfo.Irradiance );
			
	return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//Function:		ConnectCallBack
//Description:		Callback function for MQTT connection. If successful, subscribe to topic

void ConnectCallBack(struct mosquitto *mosq, void *userdata, int result)
{
	if(!result) {
		// Subscribe to topic
		mosquitto_subscribe(mosq, NULL, "Node/SBC", 0);
	} else {
		printf(stderr, "Connect failed\n");
	}

	return;
}

void DisconnectCallBack(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	//Placeholder
	//Handling of connection disconnection
	return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//Function:		PayloadParser
//Description:	Extract info from received payload such as ID, sensor value
//Assumption:	Payload format: <ProductID>#<current>#<irradiance value>

void PayloadParser (SBC_t *SbcObject , void* payload, int MsgLen)
{

	char PayLoadString[MsgLen];

	strcpy(PayLoadString, (char *)payload);

	// Payload format: <ProductID>#<current>#<irradiance value>
	strcpy(SbcObject->ProdId, strtok(PayLoadString,"\#"));	
	strcpy(SbcObject->Current, strtok(NULL,"\#"));	
	strcpy(SbcObject->Irradiance, strtok(NULL,"\#"));

	return;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
//Function:		GetSysTime
//Description:		Retrieve the system time

void GetSysTime(struct tm *SbcTime)
{
	time_t t = time(NULL);
	*SbcTime = *localtime(&t);

	//adjust date
	SbcTime->tm_year = (SbcTime->tm_year) + 1900;
	SbcTime->tm_mon = (SbcTime->tm_mon) + 1;

	return;

}


