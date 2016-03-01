#include <WinSock2.h>
#include <windows.h>
#include <ws2def.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <iostream>
#include <commctrl.h>

//my includes
#include "resource.h"
#include "GridProtocol.h"
#include "LayoutDefs.h"
#include "GridProtocol.cpp"

#define INVERTER_NAME_LENGTH              64
#define MAX_NUM_INVERTERS                 10
#define MAX_MSG_COMPONENTS                5
#define DEFAULT_INDEX                     999
#define NUM_REQUEST_SIGNALS               9

using namespace std;
/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK ConfigureDataProcedure(HWND , UINT, WPARAM, LPARAM);
//BOOL CALLBACK ToolDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
/*  Make the class name into a global variable  */

char szClassName[ ] = "Inverter Controller";

int applytoall = 0;
short int globalSEQNUM = 0x2341;
bool keepCollecting = 0;
bool verbose = true;
bool socketConfigured = false;

HWND g_configureDataProcedure = NULL;

int inverterApplyIndex = 999;
SYSTEMTIME currentTime;
SYSTEMTIME lastTime;

int counter = 0;

typedef struct{uint16_t gpid;
               uint8_t scope;
               uint8_t components;
               uint16_t seqnum;
               uint16_t typecode[5];
               double data[5];
               } GP_PACKET;

typedef struct{char IPAddress[16];
                char MACAddress[18];
                char inverterName[INVERTER_NAME_LENGTH];
                } ID_INVERTER;

typedef struct{char signalName[64];
                bool enable;
                } SIGNAL_CONF;

typedef struct{SIGNAL_CONF signal[10];
                } INV_SIGNAL_CONF;


typedef struct{short int GPcode;
                char GPstring[64];
                short int GPtype;
                } GRIDPROTO_ELEMENT;

GRIDPROTO_ELEMENT GPfunctions[38];

//initialize button handles
HWND mainCallbackHandle = NULL;
HWND enterIPEdit = NULL;
HWND enterMACEdit = NULL;
HWND enterNameEdit = NULL;
HWND createNewInvButton = NULL;
HWND sendCommandButton = NULL;
HWND inverterGroupBox = NULL;
HWND startCollectingButton = NULL;
HWND stopCollectingButton = NULL;
HWND commandListbox = NULL;
UINT timerPointer = NULL;

HWND removeEntry[MAX_NUM_INVERTERS];
HWND inverterTextIP[MAX_NUM_INVERTERS];
HWND inverterTextMAC[MAX_NUM_INVERTERS];
HWND inverterTextName[MAX_NUM_INVERTERS];
HWND inverterDataConf[MAX_NUM_INVERTERS];
HWND inverterCommandEnable[MAX_NUM_INVERTERS];
HWND inverterVoltageDisplay[MAX_NUM_INVERTERS];
HWND inverterCurrentDisplay[MAX_NUM_INVERTERS];
HWND inverterRealPowerDisplay[MAX_NUM_INVERTERS];
HWND inverterReactivePowerDisplay[MAX_NUM_INVERTERS];
HWND inverterFrequencyDisplay[MAX_NUM_INVERTERS];

HWND valueField[MAX_MSG_COMPONENTS];
HWND dataField[MAX_MSG_COMPONENTS];



ID_INVERTER zeroInverter;
//ZeroMemory(&zeroInverter,sizeof(&zeroInverter));

ID_INVERTER inverterList[10];
int inverterEnable[MAX_NUM_INVERTERS];
INV_SIGNAL_CONF inverterEnabledSignals[MAX_NUM_INVERTERS];
bool inverterCommandEnabled[MAX_NUM_INVERTERS];
int pollingInterval = 1000;  // in milliseconds

//function declarations
void ReportError(int, const char*);
int printstring(char*);
int printbuffer(char*,int);
int SendInverterCommand(int, HWND);
int FormGPIDPacket(char*,int,GP_PACKET);
int DisassembleGPIDPacket(GP_PACKET*,char*,int);
int reverseByteOrderTest();
int flipByteOrder(void*,int);
int CreateTheSocket(int);
int CheckSocket(int,GP_PACKET*);

WORD sockVersion;
WSADATA wsaData;
SOCKET theSocket;

int CreateTheSocket()
{
    int nret;

    sockVersion = MAKEWORD(2,0);
    //initialize Winsock
    WSAStartup(sockVersion,&wsaData);

    //Create the socket
    theSocket = socket(AF_INET,             //use TCP/IP
                       SOCK_DGRAM,         //datagram-oriented
                       IPPROTO_UDP);        //UDP not TCP

    WSAAsyncSelect(theSocket,mainCallbackHandle,DATA_AVAILABLE,FD_READ);
    //set socket options so that a checksum is computed
    nret = setsockopt(
                      theSocket,
                      IPPROTO_UDP,
                      UDP_CHECKSUM_COVERAGE,
                      (const char *) TRUE,
                      0
                      );
    if(theSocket == INVALID_SOCKET)
    {
        nret = WSAGetLastError();
        ReportError(nret,"socket()");
        WSACleanup();
        return NETWORK_ERROR;
    }

    socketConfigured = true;
    return 1;
}


//inverterIndex is the inverter to which we are sending the command
//commandListbox is the handle of the listbox containing the list of commands to be sent
int SendInverterCommand(int inverterIndex, HWND commandListbox)
{
    if(verbose)
    {
        cout << "BEGINNING SendInverterCommand()\n";
    }
    int rv;
    int nret;
    int found;
    char dataValueBuffer[64];
    char nameBuffer[64];
    unsigned int slen;
    unsigned int numSelections;
    unsigned int i;
    unsigned int j;
    SOCKADDR_IN serverInfo;
    GP_PACKET packet;

    if(socketConfigured == true)
    {
        cout << "socket already configured\n";
    }
    else
    {
        if(CreateTheSocket())
        {
            cout << "socket successfully created!\n";
        }
    }

    //fill a SOCKADDR_IN struct with the address information
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.S_un.S_addr = inet_addr(inverterList[inverterIndex].IPAddress);
    serverInfo.sin_port = htons(33000);

    slen = sizeof(serverInfo);

    //find out how many commands have been selected
    numSelections = SendMessage(commandListbox,LB_GETSELCOUNT,0,0);
    //find out which commands were selected
    int selBuffer[numSelections];
    SendMessage(commandListbox,LB_GETSELITEMS,numSelections,(LPARAM)&selBuffer);

    if(verbose)
    {
        cout<<"sending a message with " << numSelections << " components to: \n";
        printstring(inverterList[inverterIndex].IPAddress);
    }

    packet.gpid = GPID;
    packet.components = (uint8_t) numSelections;
    packet.seqnum = globalSEQNUM;
    packet.scope = SCOPE_INVERTER;

    cout << packet.gpid << "\n";
    cout << (int) packet.components << "\n";
    cout << packet.seqnum << "\n";
    cout << (int) packet.scope << "\n";



    for(i = 0; i < numSelections; i++)
    {
        if(verbose)
        {
            cout << "working on message " << i + 1 << "\n";
        }
        j = 0;
        found = 0;
        SendMessage(commandListbox,LB_GETTEXT,(WPARAM)selBuffer[i],(LPARAM)nameBuffer);
        printstring(nameBuffer);
        while(j < sizeof(GPfunctions)/sizeof(GPfunctions[0]) && found == 0)
        {
            if(memcmp(GPfunctions[j].GPstring,nameBuffer,strlen(nameBuffer)) == 0)
            {
                found = 1;
                packet.typecode[i] = GPfunctions[j].GPcode;
                if(verbose)
                {
                    cout << "found a match at index " << j << "\n";
                    printstring(GPfunctions[j].GPstring);
                }
                if(GPfunctions[j].GPtype == ADJUST_TYPE)
                {
                    SendMessage(dataField[i],WM_GETTEXT,64,(LPARAM)dataValueBuffer);
                    packet.data[i] = (float) atof(dataValueBuffer);
                    if(verbose)
                    {
                        cout<< "command supports a data field\n";
                        printstring(dataValueBuffer);
                        cout << packet.data[i] << "\n";
                    }
                }
            }
            j++;
        }
        packet.typecode[i] = GPfunctions[j-1].GPcode;
        cout << packet.typecode[i] << "\n";
    }


    char sendBuffer[6+10*packet.components];
    int buffersize = sizeof(sendBuffer);
    ZeroMemory(sendBuffer,buffersize);
    cout << "send buffer length is: " << sizeof(sendBuffer) << "\n";

    if(rv = FormGPIDPacket(sendBuffer, sizeof(sendBuffer), packet)!=-1)
    {
        //send/receive then cleanup
        nret = sendto(theSocket,
                      sendBuffer,
                      buffersize,
                      0,
                      (struct sockaddr *) &serverInfo,
                      slen
                      );

        if(nret == SOCKET_ERROR)
        {
            nret = WSAGetLastError();
            switch(nret)
            {
                case WSAEACCES:
                    if(verbose)
                    {
                        cout << "sendto() error 10013\n";
                    }
                break;
                default:
                    ReportError(nret,"sendto()");
                    WSACleanup();
                    return NETWORK_ERROR;
                break;
            }

        }
        cout << "sent packet\n";
        cout << nret << "\n";
        globalSEQNUM++;

        return 1;
    }
    return 0;
}

int SendScheduledRequest(int inverterIndex)
{
    if(verbose)
    {
        cout << "BEGINNING SendScheduledRequest()\n";
    }
    int rv;
    int nret;
    int found;
    char dataValueBuffer[64];
    char nameBuffer[64];
    unsigned int slen;
    unsigned int numSelections;
    unsigned int i;
    unsigned int j;
    SOCKADDR_IN serverInfo;
    GP_PACKET packet;

    if(socketConfigured == true)
    {
        cout << "socket already configured\n";
    }
    else
    {
        if(CreateTheSocket())
        {
            cout << "socket successfully created!\n";
        }
    }

    //fill a SOCKADDR_IN struct with the address information
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.S_un.S_addr = inet_addr(inverterList[inverterIndex].IPAddress);
    serverInfo.sin_port = htons(33000);

    slen = sizeof(serverInfo);

    numSelections = 0;
    for(i = 0; i < NUM_REQUEST_SIGNALS; i++)
    {
        if(inverterEnabledSignals[inverterIndex].signal[i].enable == true)
        {
            if(verbose)
            {
                cout << "working on component " << numSelections + 1 << "\n";
            }
            j = 0;
            found = 0;

            while(j < sizeof(GPfunctions)/sizeof(GPfunctions[0]) && found == 0)
            {
                if(memcmp(GPfunctions[j].GPstring,inverterEnabledSignals[inverterIndex].signal[i].signalName,strlen(inverterEnabledSignals[inverterIndex].signal[i].signalName)) == 0)
                {
                    found = 1;
                    packet.typecode[numSelections] = GPfunctions[j].GPcode;
                    packet.data[numSelections] = 0;
                    //packet.typecode[i] = GPfunctions[j].GPcode;
                    if(verbose)
                    {
                        cout << "found a match at index " << j << "\n";
                        printstring(GPfunctions[j].GPstring);
                        cout << packet.typecode[numSelections] << "\n";
                    }

                }
                j++;
            }
            numSelections++;
        }

    }

    packet.gpid = GPID;
    packet.components = (uint8_t) numSelections;
    packet.seqnum = globalSEQNUM;
    packet.scope = (uint8_t) SCOPE_INVERTER;

    cout << packet.gpid << "\n";
    cout << (int) packet.components << "\n";
    cout << packet.seqnum << "\n";
    cout << (int) packet.scope << "\n";

    char sendBuffer[6+10*packet.components];
    int buffersize = sizeof(sendBuffer);
    ZeroMemory(sendBuffer,buffersize);
    cout << "send buffer length is: " << sizeof(sendBuffer) << "\n";

    if(rv = FormGPIDPacket(sendBuffer, sizeof(sendBuffer), packet)!=-1)
    {
        //send/receive then cleanup
        nret = sendto(theSocket,
                      sendBuffer,
                      buffersize,
                      0,
                      (struct sockaddr *) &serverInfo,
                      slen
                      );

        if(nret == SOCKET_ERROR)
        {
            nret = WSAGetLastError();
            switch(nret)
            {
                case WSAEACCES:
                    if(verbose)
                    {
                        cout << "sendto() error 10013\n";
                    }
                break;
                default:
                    ReportError(nret,"sendto()");
                    WSACleanup();
                    return NETWORK_ERROR;
                break;
            }

        }
        cout << "sent packet\n";
        cout << nret << "\n";
        globalSEQNUM++;

        return 1;
    }
    else
    {
        if(verbose)
        {
            cout << "GP packet malformed!\n";
        }
    }
    return 0;
}



int FormGPIDPacket(char* sendBuffer, int buffersize, GP_PACKET packet)
{
    if(verbose)
    {
        cout << "BEGINNING FormGPIDPacket()\n";
    }
    int reverseByteOrdering;
    int i;

    //see if we need to switch byte ordering for network byte order
    reverseByteOrdering = reverseByteOrderTest();

    //convert to network byte ordering
    if(reverseByteOrderTest)
    {
        flipByteOrder(&packet.gpid,sizeof(packet.gpid));
        flipByteOrder(&packet.seqnum,sizeof(packet.seqnum));
        for(i = 0;i<packet.components;i++)
        {
            flipByteOrder(&packet.typecode[i],sizeof(packet.typecode[i]));
            flipByteOrder(&packet.data[i],sizeof(packet.data[i]));
        }
        if(verbose)
        {
            cout << "reversing byte order\n";
        }
    }

    memcpy(sendBuffer+OFFSET_GPID,&packet.gpid,2);
    cout << packet.gpid << "\n";
    memcpy(sendBuffer+OFFSET_SCOPE,&packet.scope,1);
    cout << packet.scope << "\n";
    memcpy(sendBuffer+OFFSET_COMPONENTS,&packet.components,1);
    cout << packet.components << "\n";
    memcpy(sendBuffer+OFFSET_SEQNUM,&packet.seqnum,2);
    cout << packet.seqnum << "\n";

    for(i = 0;i<packet.components;i++)
    {
        memcpy(sendBuffer+OFFSET_TYPECODE+i*10,&packet.typecode[i],2);
        memcpy(sendBuffer+OFFSET_DATA+i*10,&packet.data[i],8);
    }

    if(verbose)
    {
        cout << "\nHere is the packet:\n";
        i = printbuffer(sendBuffer,buffersize);
        cout << i << "\n";
    }

    return 1;
}

int DisassembleGPIDPacket(GP_PACKET* packetpointer, char* recBuffer, int buffersize)
{
    int i;
    int reverseByteOrdering;
    GP_PACKET packet;
    //disassemble datagram and form packet structure
    memcpy(&packet.gpid,recBuffer+OFFSET_GPID,2);
    memcpy(&packet.components,recBuffer+OFFSET_COMPONENTS,1);
    memcpy(&packet.scope,recBuffer+OFFSET_SCOPE,1);
    memcpy(&packet.seqnum,recBuffer+OFFSET_SEQNUM,2);

    for(i = 0;i < packet.components; i++)
    {
        memcpy(&packet.typecode[i],recBuffer+OFFSET_TYPECODE+10*i,2);
        memcpy(&packet.data[i],recBuffer+OFFSET_DATA+10*i,8);
    }

    //test for endian-ness
    reverseByteOrdering = reverseByteOrderTest();
    if(reverseByteOrdering)
    {
        flipByteOrder(&packet.gpid,sizeof(packet.gpid));
        flipByteOrder(&packet.seqnum,sizeof(packet.seqnum));
        for(i = 0;i<packet.components;i++)
        {
            flipByteOrder(&packet.typecode[i],sizeof(packet.typecode[i]));
            flipByteOrder(&packet.data[i],sizeof(packet.data[i]));
        }
        if(verbose)
        {
            cout << "reversing byte order\n";
        }
    }

    if(verbose)
    {
        cout << "Recieved Packet: \n";
        cout << "GPID: " <<(int) packet.gpid << " \n";
        cout << "message components: " <<(int) packet.components<< " \n";
        cout << "message scope: " <<(int) packet.scope << " \n";
        cout << "sequence number: " <<(int) packet.seqnum << " \n";

        for(i = 0; i<packet.components; i++)
        {
            cout << "typecode: " << (int) packet.typecode[i] << "\n";
            cout << "data: " << packet.data[i] << "\n";
        }

    }
    memcpy(packetpointer,&packet,sizeof(packet));
    return 1;
}

int flipByteOrder(void* pointer, int size)
{
    if(verbose)
    {
        cout << "BEGINNING flipByteOrder()\n";
    }
    int i;

    if(verbose)
    {
        cout << "flipping" << size << "bytes\n";
    }
    if(size < 9 && size > 0)
    {
        char buffer[size];
        char testbuffer[size];
        memcpy(testbuffer,pointer,size);
        for(i = 0;i<size;i++)
        {
            memcpy(buffer+i,pointer+size-i-1,1);
        }
        //copy reversed bytes to original location
        memcpy(pointer,buffer,size);
        if(verbose)
        {
            cout << "variable is\n";
            printbuffer(testbuffer,sizeof(testbuffer));
            cout << "reversed below : \n";
            printbuffer(buffer,sizeof(buffer));

        }
    }
    else
    {
        if(verbose)
        {
            cout << "problem with flipByteOrder() operand \"size\"\n";
        }
        return 0;
    }
    return 1;
}

//test byte ordering of machine to ensure proper network byte ordering
//x86 and x86-64 processors are little-endian.
//AVR32 microcontrollers are big-endian
int reverseByteOrderTest()
{
    if(verbose)
    {
        cout << "BEGINNING reverseByteOrderTest()\n";
    }

    uint32_t testint = 0xa1b2c3d4;
    uint16_t buffer1;
    ZeroMemory(&buffer1,2);
    memcpy(&buffer1,&testint,2);
    cout << "after test\n";
    cout << testint << "\n";
    cout << buffer1 << "\n";
    if(buffer1 == 0xc3d4)
    {
        if(verbose)
        {
            cout << "little endian \n";
        }
        return 1;
    }
    else if(buffer1 == 0xa1b2)
    {
        if(verbose)
        {
            cout << "big endian \n";
        }
        return 0;
    }
    else
    {
        cout << "there was a problem with the byte ordering test\n";
        return -1;
    }

}

int CheckSocket(int inverterIndex, GP_PACKET* packetpointer)
{
    int nret;
    int retv;
    int slen;
    char recBuffer[1024];
    SOCKADDR_IN serverInfo;
    GP_PACKET packet;

    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.S_un.S_addr = inet_addr(inverterList[inverterIndex].IPAddress);
    serverInfo.sin_port = htons(33000);

    slen = sizeof(serverInfo);

    nret = recvfrom(theSocket,
                    recBuffer,
                    1024,
                    0,
                    (struct sockaddr *) &serverInfo,
                    &slen
                   );

    if(nret == SOCKET_ERROR)
    {
        retv = WSAGetLastError();
        switch(retv)
        {
            case WSAEWOULDBLOCK:
                if(verbose)
                {
                    cout << "data not ready\n";
                }
            break;
            default:
                ReportError(retv,"recfrom()");
                WSACleanup();
            break;
        }
    }
    if(nret == 0)
    {
        if(verbose)
        {
            cout << "No data present\n";
        }
    }
    else
    {
        if(verbose)
        {
            cout << "DATA RECEIVED!!\n";
            printbuffer(recBuffer,nret);
        }
        DisassembleGPIDPacket(&packet,recBuffer,nret);
    }

    memcpy(packetpointer,&packet,sizeof(packet));
    return nret;
}

void displayDataConfiguration(int index)
{
    int i;
    for(i = 0; i<NUM_REQUEST_SIGNALS; i++)
    {
        cout<< inverterEnabledSignals[index].signal[i].signalName << " : " <<inverterEnabledSignals[index].signal[i].enable << "\n";
    }
}

void createGridProtocolStructs()
{
    GPfunctions[0].GPcode = MISDIRECTED_MESSAGE;
    sprintf(GPfunctions[0].GPstring,"MISDIRECTED_MESSAGE");
    GPfunctions[0].GPtype = GENERAL_TYPE;
    GPfunctions[1].GPcode = UNIMPLEMENTED_PROTOCOL;
    sprintf(GPfunctions[1].GPstring,"UNIMPLEMENTED_PROTOCOL");
    GPfunctions[1].GPtype = GENERAL_TYPE;
    GPfunctions[2].GPcode = UNIMPLEMENTED_FUNCTION;
    sprintf(GPfunctions[2].GPstring,"UNIMPLEMENTED_FUNCTION");
    GPfunctions[2].GPtype = GENERAL_TYPE;
    GPfunctions[3].GPcode = BAD_UDP_CHECKSUM;
    sprintf(GPfunctions[3].GPstring,"BAD_UDP_CHECKSUM");
    GPfunctions[3].GPtype = GENERAL_TYPE;
    GPfunctions[4].GPcode = ACK;
    sprintf(GPfunctions[4].GPstring,"ACK");
    GPfunctions[4].GPtype = GENERAL_TYPE;

    GPfunctions[5].GPcode = PHASE_ADJUST;
    sprintf(GPfunctions[5].GPstring,"PHASE_ADJUST");
    GPfunctions[5].GPtype = ADJUST_TYPE;
    GPfunctions[6].GPcode = AMPLITUDE_ADJUST;
    sprintf(GPfunctions[6].GPstring,"AMPLIITUDE_ADJUST");
    GPfunctions[6].GPtype = ADJUST_TYPE;
    GPfunctions[7].GPcode = REAL_POWER_ADJUST;
    sprintf(GPfunctions[7].GPstring,"REAL_POWER_ADJUST");
    GPfunctions[7].GPtype = ADJUST_TYPE;
    GPfunctions[8].GPcode = REACTIVE_POWER_ADJUST;
    sprintf(GPfunctions[8].GPstring,"REACTIVE_POWER_ADJUST");
    GPfunctions[8].GPtype = ADJUST_TYPE;
    GPfunctions[9].GPcode = FREQUENCY_ADJUST;
    sprintf(GPfunctions[9].GPstring,"FREQUENCY_ADJUST");
    GPfunctions[9].GPtype = ADJUST_TYPE;

    GPfunctions[10].GPcode = SET_MODE_GRID_FOLLOWING;
    sprintf(GPfunctions[10].GPstring,"SET_MODE_GRID_FOLLOWING");
    GPfunctions[10].GPtype = CONTROL_TYPE;
    GPfunctions[11].GPcode = SET_MODE_STANDALONE;
    sprintf(GPfunctions[11].GPstring,"SET_MODE_STANDALONE");
    GPfunctions[11].GPtype = CONTROL_TYPE;
    GPfunctions[12].GPcode = DISCONNECT;
    sprintf(GPfunctions[12].GPstring,"DISCONNECT");
    GPfunctions[12].GPtype = CONTROL_TYPE;
    GPfunctions[13].GPcode = CONNECT;
    sprintf(GPfunctions[13].GPstring,"CONNECT");
    GPfunctions[13].GPtype = CONTROL_TYPE;
    GPfunctions[14].GPcode = LOCK_SCREEN;
    sprintf(GPfunctions[14].GPstring,"LOCK_SCREEN");
    GPfunctions[14].GPtype = CONTROL_TYPE;
    GPfunctions[15].GPcode = UNLOCK_SCREEN;
    sprintf(GPfunctions[15].GPstring,"UNLOCK_SCREEN");
    GPfunctions[15].GPtype = CONTROL_TYPE;
    GPfunctions[16].GPcode = SCREEN1;
    sprintf(GPfunctions[16].GPstring, "SCREEN1");
    GPfunctions[16].GPtype = CONTROL_TYPE;
    GPfunctions[17].GPcode = SCREEN2;
    sprintf(GPfunctions[17].GPstring,"SCREEN2");
    GPfunctions[17].GPtype = CONTROL_TYPE;
    GPfunctions[18].GPcode = SCREEN3;
    sprintf(GPfunctions[18].GPstring,"SCREEN3");
    GPfunctions[18].GPtype = CONTROL_TYPE;
    GPfunctions[19].GPcode = SCREEN4;
    sprintf(GPfunctions[19].GPstring,"SCREEN4");
    GPfunctions[19].GPtype = CONTROL_TYPE;

    GPfunctions[20].GPcode = REQUEST_PHASE;
    sprintf(GPfunctions[20].GPstring,"REQUEST_PHASE");
    GPfunctions[20].GPtype = REQUEST_TYPE;
    GPfunctions[21].GPcode = REQUEST_AMPLITUDE;
    sprintf(GPfunctions[21].GPstring,"REQUEST_AMPLITUDE");
    GPfunctions[21].GPtype = REQUEST_TYPE;
    GPfunctions[22].GPcode = REQUEST_MODE;
    sprintf(GPfunctions[22].GPstring,"REQUEST_MODE");
    GPfunctions[22].GPtype = REQUEST_TYPE;
    GPfunctions[23].GPcode = REQUEST_PWM_PERIOD;
    sprintf(GPfunctions[23].GPstring,"REQUEST_PWM_PERIOD");
    GPfunctions[23].GPtype = REQUEST_TYPE;
    GPfunctions[24].GPcode = REQUEST_NSAMPLES;
    sprintf(GPfunctions[24].GPstring,"REQUEST_NSAMPLES");
    GPfunctions[24].GPtype = REQUEST_TYPE;
    GPfunctions[25].GPcode = REQUEST_OUTPUT_VOLTAGE;
    sprintf(GPfunctions[25].GPstring,"REQUEST_OUTPUT_VOLTAGE");
    GPfunctions[25].GPtype = REQUEST_TYPE;
    GPfunctions[26].GPcode = REQUEST_OUTPUT_CURRENT;
    sprintf(GPfunctions[26].GPstring,"REQUEST_OUTPUT_CURRENT");
    GPfunctions[26].GPtype = REQUEST_TYPE;
    GPfunctions[27].GPcode = REQUEST_REAL_POWER;
    sprintf(GPfunctions[27].GPstring,"REQUEST_REAL_POWER");
    GPfunctions[27].GPtype = REQUEST_TYPE;
    GPfunctions[28].GPcode = REQUEST_REACTIVE_POWER;
    sprintf(GPfunctions[28].GPstring,"REQUEST_REACTIVE_POWER");
    GPfunctions[28].GPtype = REQUEST_TYPE;

    GPfunctions[29].GPcode = RESPONSE_PHASE;
    sprintf(GPfunctions[29].GPstring,"RESPONSE_PHASE");
    GPfunctions[29].GPtype = RESPONSE_TYPE;
    GPfunctions[30].GPcode = RESPONSE_AMPLITUDE;
    sprintf(GPfunctions[30].GPstring,"RESPONSE_AMPLITUDE");
    GPfunctions[30].GPtype = RESPONSE_TYPE;
    GPfunctions[31].GPcode = RESPONSE_MODE;
    sprintf(GPfunctions[31].GPstring,"RESPONSE_MODE");
    GPfunctions[31].GPtype = RESPONSE_TYPE;
    GPfunctions[32].GPcode = RESPONSE_PWM_PERIOD;
    sprintf(GPfunctions[32].GPstring,"RESPONSE_PWM_PERIOD");
    GPfunctions[32].GPtype = RESPONSE_TYPE;
    GPfunctions[33].GPcode = RESPONSE_NSAMPLES;
    sprintf(GPfunctions[33].GPstring,"RESPONSE_NSAMPLES");
    GPfunctions[33].GPtype = RESPONSE_TYPE;
    GPfunctions[34].GPcode = RESPONSE_OUTPUT_VOLTAGE;
    sprintf(GPfunctions[34].GPstring,"RESPONSE_OUTPUT_VOLTAGE");
    GPfunctions[34].GPtype = RESPONSE_TYPE;
    GPfunctions[35].GPcode = RESPONSE_OUTPUT_CURRENT;
    sprintf(GPfunctions[35].GPstring,"RESPONSE_OUTPUT_CURRENT");
    GPfunctions[35].GPtype = RESPONSE_TYPE;
    GPfunctions[36].GPcode = RESPONSE_REAL_POWER;
    sprintf(GPfunctions[36].GPstring,"RESPONSE_REAL_POWER");
    GPfunctions[36].GPtype = RESPONSE_TYPE;
    GPfunctions[37].GPcode = RESPONSE_REACTIVE_POWER;
    sprintf(GPfunctions[37].GPstring,"RESPONSE_REACTIVE_POWER");
    GPfunctions[37].GPtype = RESPONSE_TYPE;
}


void resetDataConfiguration(int index)
{
    int i;
    for(i = 0; i< NUM_REQUEST_SIGNALS;i++)
    {
        inverterEnabledSignals[index].signal[i].enable = false;
    }

}

void initializeEnabledSignals()
{
    int i;
    char buffer[64];
    for(i = 0; i<MAX_NUM_INVERTERS; i++)
    {
        sprintf(buffer,"REQUEST_PHASE");
        memcpy(inverterEnabledSignals[i].signal[0].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_AMPLITUDE");
        memcpy(inverterEnabledSignals[i].signal[1].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_MODE");
        memcpy(inverterEnabledSignals[i].signal[2].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_PWM_PERIOD");
        memcpy(inverterEnabledSignals[i].signal[3].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_NSAMPLES");
        memcpy(inverterEnabledSignals[i].signal[4].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_OUTPUT_VOLTAGE");
        memcpy(inverterEnabledSignals[i].signal[5].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_OUTPUT_CURRENT");
        memcpy(inverterEnabledSignals[i].signal[6].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_REAL_POWER");
        memcpy(inverterEnabledSignals[i].signal[7].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
        sprintf(buffer,"REQUEST_REACTIVE_POWER");
        memcpy(inverterEnabledSignals[i].signal[8].signalName,buffer,strlen(buffer));
        ZeroMemory(buffer,sizeof(buffer));
    }
}

//print a char array as a string to the console
int printstring(char* stringname)
{
    int i = 0;
    while(i<strlen(stringname))
    {
        cout << stringname[i];
        i++;
    }
    cout <<"\n";
    //return number of characters printed to console
    return i;
}

//print a section of memory as characters to the console
int printbuffer(char* stringname, int size)
{
    int i = 0;
    while(i<size)
    {
        cout<<stringname[i];
        i++;
    }
    cout << "\n";
    return i;
}


//networking error handler from Johnnie Rose Jr.'s  winsock tutorial
void ReportError(int errorCode, const char *whichFunc)
{
    char errorMsg[92];          //buffer for error message
    ZeroMemory(errorMsg,92);    //automatically null-terminate string

    //The following line copies the phrase, whichFunc string, and integer errorCode into buffer
    sprintf(errorMsg,"call to %s returned error %d!",(char *)whichFunc,errorCode);
    MessageBox(NULL,errorMsg,"socketIndication",MB_OK);
}

//this function handles the configuration of data collection from inverters
BOOL CALLBACK ConfigureDataProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int retn;
    int i = 0;
    int j = 0;
    int found = 0;
    int selectionBuffer[MAX_NUM_INVERTERS] = { 999 };
    char nameBuffer[64];

    switch(msg)
    {
        case WM_CREATE:
            cout<<"created";
        break;
        case WM_LBUTTONDOWN:
            cout<<"Lbuttondown\n";
        break;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_APPLY:
                    //are these settings to be applied to all?
                    retn = SendDlgItemMessage(g_configureDataProcedure,IDC_APPLYALL,BM_GETCHECK,0,0);
                    if(retn == BST_CHECKED)
                    {
                        cout << "apply to all\n";
                        applytoall = 1;
                    }
                    else if(retn == BST_UNCHECKED)
                    {
                        cout << "don't apply to all\n";
                        applytoall = 0;
                    }
                    else
                    {
                        applytoall = 0;
                    }


                    if(applytoall == 1)
                    {
                        for(i = 0;i < MAX_NUM_INVERTERS;i++)
                        {
                            //remember to fix this!
                        }
                    }
                    else
                    {
                        cout << inverterApplyIndex << "\n";
                        if(inverterApplyIndex!=999)
                        {
                            retn = SendDlgItemMessage(g_configureDataProcedure,IDC_LISTALL,LB_GETSELITEMS,MAX_NUM_INVERTERS,(LPARAM)selectionBuffer);
                            if(retn == LB_ERR)
                            {
                                cout << "listbox error\n";
                                return FALSE;
                            }
                            retn = SendDlgItemMessage(g_configureDataProcedure,IDC_LISTALL,LB_GETSELCOUNT,0,0);
                            while(i < NUM_REQUEST_SIGNALS && found <= retn)
                            {
                                inverterEnabledSignals[inverterApplyIndex].signal[i].enable = false;
                                for(j = 0; j < retn; j++)
                                {
                                    ZeroMemory(nameBuffer,sizeof(nameBuffer));
                                    SendDlgItemMessage(g_configureDataProcedure,IDC_LISTALL,LB_GETTEXT,(WPARAM)selectionBuffer[j],(LPARAM)nameBuffer);
                                    cout << i << " " << j << "\n";
                                    printstring(nameBuffer);
                                    if(!memcmp(nameBuffer,inverterEnabledSignals[inverterApplyIndex].signal[i].signalName,strlen(nameBuffer)))
                                    {
                                         inverterEnabledSignals[inverterApplyIndex].signal[i].enable = true;
                                         cout << "match at " << j << "\n";
                                         found++;
                                    }
                                }
                                i++;
                            }
                        }
                    }
                    displayDataConfiguration(inverterApplyIndex);
                break;
                case IDC_APPLYALL:
                    retn = SendDlgItemMessage(g_configureDataProcedure,IDC_APPLYALL,BM_GETCHECK,0,0);
                    if(retn == BST_CHECKED)
                    {
                        applytoall = 1;
                         SendDlgItemMessage(g_configureDataProcedure,IDC_APPLYALL,BM_SETCHECK,BST_UNCHECKED,0);

                    }
                    else if(retn == BST_UNCHECKED)
                    {
                        applytoall = 0;
                        SendDlgItemMessage(g_configureDataProcedure,IDC_APPLYALL,BM_SETCHECK,BST_CHECKED,0);

                    }
                    else
                    {
                        applytoall = 0;
                        cout << "checkboxdisabled\n";
                    }
                break;
                case IDC_OK:
                    SendMessage(hwnd,WM_COMMAND,(WPARAM) IDC_APPLY,(LPARAM) 0);
                    DestroyWindow(hwnd);
                break;
                case IDC_CANCEL:
                    DestroyWindow(hwnd);
                break;
                case LBN_SELCHANGE:

                break;
                default:
                    return FALSE;
                break;
            }
        break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
        break;
        default:
            return FALSE;
        break;
    }
    return FALSE;
}

/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowsProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int index = 0;
    int data = 0;
    BOOL WINAPI boodata = 0;
    int i = 0;
    int j = 0;
    int nret = 0;
    int inverterInsertIndex = DEFAULT_INDEX;
    int inverterRemoveIndex;
    int timeDifference = 0;
    int selBuffer[MAX_MSG_COMPONENTS+3];
    int found;

    UINT_PTR timerEvent = 1;
    GP_PACKET packet;

    char commandBuffer[256];
    char dispbuffer[64];
    char inputBuffer[64];
    char IPBuffer[16];
    char MACBuffer[18];
    char nameBuffer[INVERTER_NAME_LENGTH];

    char remCommand1 [] ="netsh interface ipv4 delete neighbors \"Ethernet\" \"";
    char addCommand1 [] ="netsh interface ipv4 add neighbors \"Ethernet\" \"";
    char command2 [] ="\" \"";
    char command3 [] ="\"";

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    switch(msg)
    {
    case WM_CREATE:
        //cout << "this happened";
        sendCommandButton = CreateWindowEx(NULL,
                                               "BUTTON",
                                               "Send",
                                               WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                                               350,310,100,24,
                                               hwnd,
                                               (HMENU)IDC_SEND_BUTTON,
                                               GetModuleHandle(NULL),
                                               NULL);
        createNewInvButton = CreateWindowEx(NULL,
                                               "BUTTON",
                                               "Create",
                                               WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                                               540,ROW1_YLOC,100,24,
                                               hwnd,
                                               (HMENU)IDC_NEWINV_BUTTON,
                                               GetModuleHandle(NULL),
                                               NULL);
        enterIPEdit = CreateWindowEx(NULL,
                                     "EDIT",
                                     "192.168.0.24",   //set for debugging purposes
                                     WS_TABSTOP|WS_CHILD|WS_VISIBLE,
                                     40,ROW1_YLOC,110,24,
                                     hwnd,
                                     (HMENU)IDC_IPEDIT,
                                     GetModuleHandle(NULL),
                                     NULL);
        enterMACEdit = CreateWindowEx(NULL,
                                  "EDIT",
                                  "00-04-25-41-56-52",   //set for debugging purposes
                                  WS_TABSTOP|WS_CHILD|WS_VISIBLE,
                                  160,ROW1_YLOC,120,24,
                                  hwnd,
                                  (HMENU)IDC_MACEDIT,
                                  GetModuleHandle(NULL),
                                  NULL);
        enterNameEdit = CreateWindowEx(NULL,
                                       "EDIT",
                                       "george",             //set for debugging purposes
                                       WS_TABSTOP|WS_CHILD|WS_VISIBLE,
                                       290,ROW1_YLOC,180,24,
                                       hwnd,
                                       (HMENU) IDC_NAMEEDIT,
                                       GetModuleHandle(NULL),
                                       NULL);
        inverterGroupBox = CreateWindow("Button",
                                          "Active Inverters",
                                          WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
                                          30,INVDISP_ROW1-22,1080,330,
                                          hwnd,
                                          NULL,
                                          GetModuleHandle(NULL),
                                          NULL);
        startCollectingButton = CreateWindowEx(NULL,
                                               "BUTTON",
                                               "Start",
                                               WS_TABSTOP|WS_CHILD|WS_VISIBLE,
                                               560,ROW1_YLOC+34,100,24,
                                               hwnd,
                                               (HMENU) IDC_START_COLLECTING,
                                               GetModuleHandle(NULL),
                                               NULL);
        stopCollectingButton = CreateWindowEx(NULL,
                                              "BUTTON",
                                              "Stop",
                                              WS_TABSTOP|WS_CHILD|WS_VISIBLE,
                                              660,ROW1_YLOC+34,100,24,
                                              hwnd,
                                              (HMENU) IDC_STOP_COLLECTING,
                                              GetModuleHandle(NULL),
                                              NULL);
        commandListbox = CreateWindowEx(NULL,
                                        "LISTBOX",
                                        "",
                                        WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY|LBS_MULTIPLESEL,
                                        10,ROW1_YLOC+60,260,200,
                                        hwnd,
                                        (HMENU)IDC_INVCOMMAND_LIST,
                                        GetModuleHandle(NULL),
                                        NULL);
        for(i = 0; i < MAX_MSG_COMPONENTS; i++)
        {
            valueField[i] = CreateWindowEx(NULL,
                                            "EDIT",
                                            "",
                                            WS_TABSTOP|WS_CHILD|WS_VISIBLE,
                                            MSG_COMP_XLOC,ROW1_YLOC+60+34*i,180,24,
                                            hwnd,
                                            (HMENU) NULL,
                                            GetModuleHandle(NULL),
                                            NULL);
            SendMessage(valueField[i],EM_SETREADONLY,(WPARAM)TRUE,NULL);
            dataField[i] = CreateWindowEx(NULL,
                                          "EDIT",
                                          "",
                                          WS_TABSTOP|WS_CHILD|WS_VISIBLE,
                                          MSG_DATA_XLOC,ROW1_YLOC+60+34*i,180,24,
                                          hwnd,
                                          (HMENU) NULL,
                                          GetModuleHandle(NULL),
                                          NULL);
        }

        //populate command listbox
        for(i = 0; i < sizeof(GPfunctions)/sizeof(GPfunctions[0]); i++)
        {
            SendMessage(commandListbox,LB_INSERTSTRING,(WPARAM)i,(LPARAM)GPfunctions[i].GPstring);
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_COMMAND:
    {
        switch(LOWORD(wParam))
        {
        case IDC_INVCOMMAND_LIST:
            cout << "inverter command listbox: " << HIWORD(wParam) << "\n";
            switch(HIWORD(wParam))
            {
                case LBN_SELCHANGE:
                    cout << "run\n";
                    nret = SendMessage(commandListbox,LB_GETSELCOUNT,NULL,NULL);
                    SendMessage(commandListbox,LB_GETSELITEMS,(WPARAM)MAX_MSG_COMPONENTS+2,(LPARAM)selBuffer);
                    while(nret > MAX_MSG_COMPONENTS)
                    {
                        SendMessage(commandListbox,LB_SETSEL,(WPARAM) FALSE ,(LPARAM)selBuffer[0]);
                        nret = SendMessage(commandListbox,LB_GETSELCOUNT,NULL,NULL);
                        SendMessage(commandListbox,LB_GETSELITEMS,(WPARAM)MAX_MSG_COMPONENTS+2,(LPARAM)selBuffer);
                    }
                    for(i = 0; i < MAX_MSG_COMPONENTS; i++)
                    {
                        if(i < nret)
                        {
                            SendMessage(commandListbox,LB_GETTEXT,(WPARAM)selBuffer[i],(LPARAM)commandBuffer);
                        }
                        else
                        {
                            ZeroMemory(commandBuffer,sizeof(commandBuffer));
                        }
                        SendMessage(valueField[i],WM_SETTEXT,0,(LPARAM)commandBuffer);
                    }
                break;
            }
        break;
        case IDC_SEND_BUTTON:
            if(verbose)
            {
                cout<<"sending a command\n";
            }
            //send a message to all inverters selected to receive the message
            for(i = 0; i<MAX_NUM_INVERTERS; i++)
            {
                if(inverterCommandEnabled[i] == true)
                {
                    if(verbose)
                    {
                        cout<<"to inverter number: " << i << "\n";
                    }
                    SendInverterCommand(i,commandListbox);
                }
            }

            break;
        case IDC_NEWINV_BUTTON:
            ZeroMemory(IPBuffer,sizeof(IPBuffer));
            ZeroMemory(MACBuffer,sizeof(MACBuffer));
            ZeroMemory(nameBuffer,sizeof(nameBuffer));

            nret = SendMessage(enterIPEdit,
                               WM_GETTEXT,
                               sizeof(IPBuffer),
                               reinterpret_cast<LPARAM>(IPBuffer));
            printstring(IPBuffer);
            ZeroMemory(MACBuffer,sizeof(MACBuffer));

            nret = SendMessage(enterMACEdit,
                               WM_GETTEXT,
                               sizeof(MACBuffer),
                               reinterpret_cast<LPARAM>(MACBuffer));
            printstring(MACBuffer);

            nret = SendMessage(enterNameEdit,
                               WM_GETTEXT,
                               sizeof(nameBuffer),
                               reinterpret_cast<LPARAM>(nameBuffer));
            printstring(nameBuffer);

            //find first empty entry in the inverterList structure
            for(i = 0;i<MAX_NUM_INVERTERS;i++)
            {
                if(memcmp(&inverterList[i],&zeroInverter,sizeof(zeroInverter)) == 0)
                {
                    inverterInsertIndex = i;
                    break;
                }
            }
            if(inverterInsertIndex == DEFAULT_INDEX)
            {
                cout << "inverter limit reached!\n";
            }

            cout << "adding a new inverter at " << inverterInsertIndex << "\n";
            //create new entry in inverterlist
            memcpy(inverterList[inverterInsertIndex].IPAddress,IPBuffer,sizeof(IPBuffer));
            memcpy(inverterList[inverterInsertIndex].MACAddress,MACBuffer,sizeof(MACBuffer));
            memcpy(inverterList[inverterInsertIndex].inverterName,nameBuffer,sizeof(nameBuffer));

            //create edit controls for displaying info about this inverter
            removeEntry[inverterInsertIndex] = CreateWindowEx(NULL,
                                         "BUTTON",
                                         "REMOVE",
                                         WS_CHILD|WS_VISIBLE,
                                         40,(int)INVDISP_ROW1+30*inverterInsertIndex,80,24,
                                         hwnd,
                                         (HMENU)IDC_REMOVE,
                                         GetModuleHandle(NULL),
                                         NULL);
            inverterTextName[inverterInsertIndex] = CreateWindowEx(NULL,
                                          "EDIT",
                                          inverterList[inverterInsertIndex].inverterName,
                                          WS_CHILD|WS_VISIBLE,
                                          130,(int)INVDISP_ROW1+30*inverterInsertIndex,180,24,
                                          hwnd,
                                          (HMENU)IDC_STATIC,
                                          GetModuleHandle(NULL),
                                          NULL);
            inverterTextIP[inverterInsertIndex] = CreateWindowEx(NULL,
                                          "EDIT",
                                          inverterList[inverterInsertIndex].IPAddress,
                                          WS_CHILD|WS_VISIBLE,
                                          320,(int)INVDISP_ROW1+30*inverterInsertIndex,110,24,
                                          hwnd,
                                          (HMENU)IDC_STATIC,
                                          GetModuleHandle(NULL),
                                          NULL);
            inverterTextMAC[inverterInsertIndex] = CreateWindowEx(NULL,
                                          "EDIT",
                                          inverterList[inverterInsertIndex].MACAddress,
                                          WS_CHILD|WS_VISIBLE,
                                          440,(int)INVDISP_ROW1+30*inverterInsertIndex,120,24,
                                          hwnd,
                                          (HMENU) IDC_STATIC,
                                          GetModuleHandle(NULL),
                                          NULL);
            inverterDataConf[inverterInsertIndex] = CreateWindowEx(NULL,
                                                                   "BUTTON",
                                                                   "CONF",
                                                                   WS_CHILD|WS_VISIBLE,
                                                                   570,(int)INVDISP_ROW1+30*inverterInsertIndex,80,24,
                                                                   hwnd,
                                                                   (HMENU) ID_CONFIGURE_DATA,
                                                                   GetModuleHandle(NULL),
                                                                   NULL);
            inverterCommandEnable[inverterInsertIndex] = CreateWindowEx(NULL,
                                                                        "BUTTON",
                                                                        "command enable",
                                                                        WS_VISIBLE|WS_CHILD|BS_CHECKBOX,
                                                                        680,(int)INVDISP_ROW1+30*inverterInsertIndex,10,10,
                                                                        hwnd,
                                                                        (HMENU) IDC_COMMAND_ENABLE,
                                                                        GetModuleHandle(NULL),
                                                                        NULL);
            inverterVoltageDisplay[inverterInsertIndex] = CreateWindowEx(NULL,
                                                                         "EDIT",
                                                                         "Voltage",
                                                                         WS_CHILD|WS_VISIBLE,
                                                                         700,(int)INVDISP_ROW1+30*inverterInsertIndex,DISPLAY_XDIM,DISPLAY_YDIM,
                                                                         hwnd,
                                                                         (HMENU)IDC_COMMAND_ENABLE,
                                                                         GetModuleHandle(NULL),
                                                                         NULL);
            inverterCurrentDisplay[inverterInsertIndex] = CreateWindowEx(NULL,
                                                                         "EDIT",
                                                                         "Current",
                                                                         WS_CHILD|WS_VISIBLE,
                                                                         (int) 700+DISPLAY_XDIM+10,(int)INVDISP_ROW1+30*inverterInsertIndex,DISPLAY_XDIM,DISPLAY_YDIM,
                                                                         hwnd,
                                                                         (HMENU)IDC_COMMAND_ENABLE,
                                                                         GetModuleHandle(NULL),
                                                                         NULL);
            inverterRealPowerDisplay[inverterInsertIndex] = CreateWindowEx(NULL,
                                                                         "EDIT",
                                                                         "P",
                                                                         WS_CHILD|WS_VISIBLE,
                                                                         (int) 700+2*(DISPLAY_XDIM+10),(int)INVDISP_ROW1+30*inverterInsertIndex,DISPLAY_XDIM,DISPLAY_YDIM,
                                                                         hwnd,
                                                                         (HMENU)IDC_COMMAND_ENABLE,
                                                                         GetModuleHandle(NULL),
                                                                         NULL);
            inverterReactivePowerDisplay[inverterInsertIndex] = CreateWindowEx(NULL,
                                                                         "EDIT",
                                                                         "Q",
                                                                         WS_CHILD|WS_VISIBLE,
                                                                         (int) 700+3*(DISPLAY_XDIM+10),(int)INVDISP_ROW1+30*inverterInsertIndex,DISPLAY_XDIM,DISPLAY_YDIM,
                                                                         hwnd,
                                                                         (HMENU)IDC_COMMAND_ENABLE,
                                                                         GetModuleHandle(NULL),
                                                                         NULL);
            inverterFrequencyDisplay[inverterInsertIndex] = CreateWindowEx(NULL,
                                                                         "EDIT",
                                                                         "period",
                                                                         WS_CHILD|WS_VISIBLE,
                                                                         (int) 700+4*(DISPLAY_XDIM+10),(int)INVDISP_ROW1+30*inverterInsertIndex,DISPLAY_XDIM,DISPLAY_YDIM,
                                                                         hwnd,
                                                                         (HMENU)IDC_COMMAND_ENABLE,
                                                                         GetModuleHandle(NULL),
                                                                         NULL);
            //set edit controls to read only
            SendMessage(inverterTextName[inverterInsertIndex],
                               EM_SETREADONLY,
                               (WPARAM) TRUE,
                               NULL);  //doesn't matter
            SendMessage(inverterTextIP[inverterInsertIndex],
                               EM_SETREADONLY,
                               (WPARAM) TRUE,
                               NULL);
            SendMessage(inverterTextMAC[inverterInsertIndex],
                               EM_SETREADONLY,
                               (WPARAM) TRUE,
                               NULL);
            SendMessage(inverterVoltageDisplay[inverterInsertIndex],
                        EM_SETREADONLY,
                        (WPARAM) TRUE,
                        NULL);
            SendMessage(inverterCurrentDisplay[inverterInsertIndex],
                        EM_SETREADONLY,
                        (WPARAM) TRUE,
                        NULL);
            SendMessage(inverterRealPowerDisplay[inverterInsertIndex],
                        EM_SETREADONLY,
                        (WPARAM) TRUE,
                        NULL);
            SendMessage(inverterReactivePowerDisplay[inverterInsertIndex],
                        EM_SETREADONLY,
                        (WPARAM) TRUE,
                        NULL);
            SendMessage(inverterFrequencyDisplay[inverterInsertIndex],
                        EM_SETREADONLY,
                        (WPARAM) TRUE,
                        NULL);
            //create the command line command string to add the specified inverter
            // to the ARP cache
            ZeroMemory(commandBuffer,sizeof(commandBuffer));
            memcpy(commandBuffer,addCommand1,strlen(addCommand1));
            memcpy(commandBuffer+strlen(commandBuffer),IPBuffer,strlen(IPBuffer));
            memcpy(commandBuffer+strlen(commandBuffer),command2,strlen(command2));
            memcpy(commandBuffer+strlen(commandBuffer),MACBuffer,strlen(MACBuffer));
            memcpy(commandBuffer+strlen(commandBuffer),command3,strlen(command3));

            cout<<"\n";
            cout<<strlen(IPBuffer) << "\n";
            cout<<strlen(MACBuffer) << "\n";

            printstring(commandBuffer);
            // use the networking shell to add the inverter to the arp cache
            if(!CreateProcess(NULL,
               commandBuffer,
               NULL,
               NULL,
               FALSE,
               0,
               NULL,
               NULL,
               &si,
               &pi))
             {
                printf("CreateProcess failed (%d).\n", GetLastError());
             }
            break;
        case IDC_REMOVE:
            inverterRemoveIndex = (int)lParam;
            for(i=0;i<MAX_NUM_INVERTERS;i++)
            {
                if((int)removeEntry[i] == (int)lParam)
                {
                    inverterRemoveIndex = i;
                }
            }
            cout << "removing an inverter at " << inverterRemoveIndex << "\n";

            //build command line command string to
            //remove specified inverter information from ARP cache
            ZeroMemory(commandBuffer,sizeof(commandBuffer));
            memcpy(commandBuffer,remCommand1,strlen(remCommand1));
            memcpy(commandBuffer+strlen(commandBuffer),inverterList[inverterRemoveIndex].IPAddress,strlen(inverterList[inverterRemoveIndex].IPAddress));
            memcpy(commandBuffer+strlen(commandBuffer),command3,strlen(command3));

            //use command line process to remove
            if(!CreateProcess(NULL,
               commandBuffer,
               NULL,
               NULL,
               FALSE,
               0,
               NULL,
               NULL,
               &si,
               &pi))
             {
                printf("CreateProcess failed (%d).\n", GetLastError());
             }

            DestroyWindow(removeEntry[inverterRemoveIndex]);
            DestroyWindow(inverterTextMAC[inverterRemoveIndex]);
            DestroyWindow(inverterTextIP[inverterRemoveIndex]);
            DestroyWindow(inverterTextName[inverterRemoveIndex]);
            DestroyWindow(inverterDataConf[inverterRemoveIndex]);
            DestroyWindow(inverterCommandEnable[inverterRemoveIndex]);
            DestroyWindow(inverterVoltageDisplay[inverterRemoveIndex]);
            DestroyWindow(inverterCurrentDisplay[inverterRemoveIndex]);
            DestroyWindow(inverterRealPowerDisplay[inverterRemoveIndex]);
            DestroyWindow(inverterReactivePowerDisplay[inverterRemoveIndex]);
            DestroyWindow(inverterFrequencyDisplay[inverterRemoveIndex]);
            ZeroMemory(&inverterList[inverterRemoveIndex],sizeof(inverterList[inverterRemoveIndex]));
            resetDataConfiguration(inverterRemoveIndex);

            break;
        case ID_CONFIGURE_DATA:
            //find out which inverter's settings we're changing
            while(i<MAX_NUM_INVERTERS && inverterDataConf[i]!=NULL)
            {
                if((int)inverterDataConf[i] == (int)lParam)
                {
                    inverterApplyIndex = i;
                    cout << inverterApplyIndex << "\n";
                }
                i++;
            }
            g_configureDataProcedure = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FUNCTIONADD), hwnd, ConfigureDataProcedure);

            if(g_configureDataProcedure == NULL)
            {
                MessageBox(hwnd, "CreateDialog returned NULL", "Warning!", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                ShowWindow(g_configureDataProcedure,SW_SHOWDEFAULT);
            }

            // create signal list
            for(i = 0;i<NUM_REQUEST_SIGNALS;i++)
            {
                SendDlgItemMessage(g_configureDataProcedure,IDC_LISTALL,LB_INSERTSTRING,(WPARAM) i, (LPARAM) inverterEnabledSignals[0].signal[i].signalName);
            }
            //highlight already selected signals
            for(i = 0;i<NUM_REQUEST_SIGNALS;i++)
            {
                if(inverterEnabledSignals[inverterApplyIndex].signal[i].enable == true)
                {
                    SendDlgItemMessage(g_configureDataProcedure,IDC_LISTALL,LB_SETSEL,TRUE,(LPARAM)i);
                }
                else
                {
                    SendDlgItemMessage(g_configureDataProcedure,IDC_LISTALL,LB_SETSEL,FALSE,(LPARAM)i);
                }
            }

            break;
        case IDC_START_COLLECTING:
            timerPointer = SetTimer(hwnd,
                                    timerEvent,
                                    1000,
                                    NULL);
            if(timerPointer == NULL)
            {
                cout << "timer was not created!\n";
            }
            else
            {
                cout << "created new timer with handle " << timerPointer << "\n";
            }

            cout<<"timer created!\n";
        break;
        case IDC_STOP_COLLECTING:
            cout<<"IDC_STOP_COLLECTING\n";
            if(KillTimer(hwnd,timerEvent))
            {
                cout<<"timer destroyed!\n";
            }
            else
            {
                cout<<"kill timer failed\n";
            }
        break;
        case IDC_COMMAND_ENABLE:
            //find out which inverter's settings we're changing
            while(i<MAX_NUM_INVERTERS && inverterCommandEnable[i]!=NULL)
            {
                if((int)inverterCommandEnable[i] == (int)lParam)
                {
                    inverterApplyIndex = i;
                    break;
                }
                i++;
            }
            if(verbose)
            {
                cout << inverterApplyIndex << "\n";
            }
            //toggle checkbox and record value
            nret = SendMessage(inverterCommandEnable[inverterApplyIndex],BM_GETCHECK,0,0);
            if(nret == BST_CHECKED)
            {
                inverterCommandEnabled[inverterApplyIndex] = false;
                SendMessage(inverterCommandEnable[inverterApplyIndex],BM_SETCHECK,BST_UNCHECKED,0);

            }
            else if(nret == BST_UNCHECKED)
            {
                inverterCommandEnabled[inverterApplyIndex] = true;
                SendMessage(inverterCommandEnable[inverterApplyIndex],BM_SETCHECK,BST_CHECKED,0);

            }
            else
            {
                inverterCommandEnabled[inverterApplyIndex] = false;
                cout << "checkboxdisabled\n";
            }

        break;
        case ID_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE,0,0);
            break;
        case ID_STUFF_GO:
            PostMessage(hwnd, WM_MBUTTONDOWN,0,0);
            break;
        case ID_FILE_NEW:

            break;
        case ID_FILE_SAVEAS:

            break;
        case ID_FILE_OPEN:

            break;
        default:
                return 0;
        break;

        }
    }
    break;
    case WM_TIMER:
        GetSystemTime(&currentTime);
        timeDifference = 1000*((int)currentTime.wSecond - (int)lastTime.wSecond);
        timeDifference += (int)currentTime.wMilliseconds - (int)lastTime.wMilliseconds;
        if(verbose)
        {
            cout<<"timer rolled over after " << timeDifference << " ms\n";
        }

        //check the socket to see if we've recieved a reply from any inverters
        for(i = 0; i<MAX_NUM_INVERTERS; i++)
        {
            SendScheduledRequest(i);
        }

        lastTime = currentTime;
    break;
    case DATA_AVAILABLE:
        //check the socket to see if we've recieved a reply from any inverters
        for(i = 0; i<MAX_NUM_INVERTERS; i++)
        {
            nret = CheckSocket(i,&packet);
            cout<< "this is the packet size: " << sizeof(packet) << "\n";
            cout << (int) packet.components << "\n";
            for(j = 0; j < packet.components; j++)
            {
                if(packet.typecode[j] == RESPONSE_OUTPUT_VOLTAGE)
                {
                    cout<<"VOLTAGE\n";
                    snprintf(nameBuffer,sizeof(nameBuffer),"%6.3f V",packet.data[j]);
                    SendMessage(inverterVoltageDisplay[i],WM_SETTEXT,(WPARAM)NULL,(LPARAM)nameBuffer);
                }
                else if(packet.typecode[j] == RESPONSE_OUTPUT_CURRENT)
                {
                    snprintf(nameBuffer,sizeof(nameBuffer),"%6.3f A",packet.data[j]);
                    SendMessage(inverterCurrentDisplay[i],WM_SETTEXT,(WPARAM)NULL,(LPARAM)nameBuffer);
                }
                else if(packet.typecode[j] == RESPONSE_REAL_POWER)
                {
                    snprintf(nameBuffer,sizeof(nameBuffer),"%6.3f W",packet.data[j]);
                    SendMessage(inverterRealPowerDisplay[i],WM_SETTEXT,(WPARAM)NULL,(LPARAM)nameBuffer);
                }
                else if(packet.typecode[j] == RESPONSE_REACTIVE_POWER)
                {
                    snprintf(nameBuffer,sizeof(nameBuffer),"%6.3f VAR",packet.data[j]);
                    SendMessage(inverterReactivePowerDisplay[i],WM_SETTEXT,(WPARAM)NULL,(LPARAM)nameBuffer);
                }
                else if(packet.typecode[j] == RESPONSE_NSAMPLES)
                {
                    snprintf(nameBuffer,sizeof(nameBuffer),"%4.0f ",packet.data[j]);
                    SendMessage(inverterFrequencyDisplay[i],WM_SETTEXT,(WPARAM)NULL,(LPARAM)nameBuffer);
                }
                else
                {
                    cout << "something else\n";
                }
            }

        }
    break;
    case WM_DESTROY:
    {
        closesocket(theSocket);
        WSACleanup();
        cout << "socket cleaned up\n";
        PostQuitMessage(0);
    }
    break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nCmdShow)
{
    //initialize data collection configuration settings
    initializeEnabledSignals();
    //initialize grid protocol signal list structure
    createGridProtocolStructs();


    HWND hwnd;               /* This is the handle for our window */
    MSG messages;            /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */

    /* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowsProcedure;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
    wincl.cbSize = sizeof (WNDCLASSEX);

    /* Use default icon and mouse-pointer */
    wincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 /* No menu */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    /* Use Windows's default colour as the background of the window */
    wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;
    wincl.lpszMenuName = MAKEINTRESOURCE(IDR_MYMENU);
    wincl.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MYICON));
    wincl.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_MYICON),IMAGE_ICON, 16,16,0);
    /* Register the window class, and if it fails quit the program */
    if (!RegisterClassEx (&wincl))
        return 0;


    /* The class is registered, let's create the program*/
    hwnd = CreateWindowEx (
               0,                   /* Extended possibilites for variation */
               szClassName,         /* Classname */
               "Inverter Controller",       /* Title Text */
               WS_OVERLAPPEDWINDOW | WS_TABSTOP, /* default window */
               CW_USEDEFAULT,       /* Windows decides the position */
               CW_USEDEFAULT,       /* where the window ends up on the screen */
               1200,                 /* The programs width */
               800,                 /* and height in pixels */
               HWND_DESKTOP,        /* The window is a child-window to desktop */
               NULL,                /* No menu */
               hThisInstance,       /* Program Instance handler */
               NULL                 /* No Window Creation data */
           );

    mainCallbackHandle = hwnd;

    /* Make the window visible on the screen */
    ShowWindow (hwnd, nCmdShow);
    /* Run the message loop. It will run until GetMessage() returns 0 */
    while (GetMessage (&messages, NULL, 0, 0))
    {
        if(!IsDialogMessage(g_configureDataProcedure, &messages))
        {
            /* Translate virtual-key messages into character messages */
            TranslateMessage(&messages);
            /* Send message to WindowProcedure */
            DispatchMessage(&messages);
        }
    }

    /* The program return-value is 0 - The value that PostQuitMessage() gave */
    return messages.wParam;
}

