/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "app_commands.h"
#include <wolfssl/ssl.h>
#include <tcpip/src/hash_fnv.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions.
*/


// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

int32_t _APP_ParseUrl(char *uri, char **host, char **path, uint16_t * port);
int32_t _APP_ParseIPPort(char *ipPort, char **ip, TCP_PORT *port);
NET_PRES_SIGNAL_FUNCTION _APP_MessageReceiveHandler(void);
char* _APP_ParseMessage (char* message);
// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************
#ifndef htons

uint16_t htons(uint16_t n) {
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}
#endif

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void) {
    memset(&appData, 0, sizeof (appData));
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;
    APP_Commands_Init();
    appData.ipMode = 4;
}

uint8_t times_called = 0;
/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */
char networkBuffer[256];

void APP_Tasks(void) {
    /* Check the application's current state. */
    switch (appData.state) {
            /* Application's initial state. */
        case APP_STATE_INIT:
        {
            //Clear Console
            SYS_CONSOLE_PRINT("%c%c%c%c",0x1B,0x5B,0x32,0x4A);
            appData.state = APP_TCPIP_WAIT_FOR_TCPIP_INIT;
            SYS_CONSOLE_MESSAGE(" APP: Initialization\r\n");
            break;
        }

        case APP_TCPIP_WAIT_FOR_TCPIP_INIT:
        {
            SYS_STATUS tcpipStat;
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            const char *netName, *netBiosName;
            int i, nNets;
            TCPIP_NET_HANDLE netH;
            if (tcpipStat < 0) { // some error occurred
                SYS_CONSOLE_MESSAGE(" APP: TCP/IP stack initialization failed!\r\n");
                appData.state = APP_TCPIP_ERROR;
            } else if (tcpipStat == SYS_STATUS_READY) {
                // now that the stack is ready we can check the
                // available interfaces
                nNets = TCPIP_STACK_NumberOfNetworksGet();
                for (i = 0; i < nNets; i++) {

                    netH = TCPIP_STACK_IndexToNet(i);
                    netName = TCPIP_STACK_NetNameGet(netH);
                    netBiosName = TCPIP_STACK_NetBIOSName(netH);

#if defined(TCPIP_STACK_USE_NBNS)
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS enabled\r\n", netName, netBiosName);
#else
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS disabled\r\n", netName, netBiosName);
#endif  // defined(TCPIP_STACK_USE_NBNS)
                    (void)netName;          // avoid compiler warning 
                    (void)netBiosName;      // if SYS_CONSOLE_PRINT is null macro

                }
                appData.state = APP_TCPIP_WAIT_FOR_IP;

            }
            break;
        }
        case APP_TCPIP_WAIT_FOR_IP:
        {
            int i, nNets;
            TCPIP_NET_HANDLE netH;
            nNets = TCPIP_STACK_NumberOfNetworksGet();
            for (i = 0; i < nNets; i++) {
                netH = TCPIP_STACK_IndexToNet(i);
                if (!TCPIP_STACK_NetIsReady(netH)) {
                    return; // interface not ready yet!
                }
                IPV4_ADDR ipAddr;
                ipAddr.Val = TCPIP_STACK_NetAddress(netH);
                SYS_CONSOLE_MESSAGE(TCPIP_STACK_NetNameGet(netH));
                SYS_CONSOLE_MESSAGE(" IP Address: ");
                SYS_CONSOLE_PRINT("%d.%d.%d.%d \r\n", ipAddr.v[0], ipAddr.v[1], ipAddr.v[2], ipAddr.v[3]);
            }
            // all interfaces ready. Could start transactions!!!
            appData.state = APP_TCPIP_WAITING_FOR_COMMAND;
            //... etc.
            break;
        }
        case APP_TCPIP_WAITING_FOR_COMMAND:
            break;
        case APP_TCPIP_PARSE_IP_PORT:
        {
            //Parse IP and Port then check if data is correct
            _APP_ParseIPPort(appData.ipPortBuffer,&appData.host,&appData.port);

            //Check if Port is valid
            if(appData.port <= 0 || appData.port > 65535)
            {
                SYS_CONSOLE_MESSAGE("Invalid PORT\r\n");
                appData.state = APP_TCPIP_WAITING_FOR_COMMAND;
                break;
            }
            
            //Convert String IP to an IPV4_ADDR struct and check if IP is valid
            if(TCPIP_Helper_StringToIPAddress(appData.host, &appData.address.v4Add) == false)
            {
                SYS_CONSOLE_MESSAGE("Invalid IP\r\n");
                appData.state = APP_TCPIP_WAITING_FOR_COMMAND;
                break;
            }
            
            SYS_CONSOLE_PRINT("IP: %s, PORT: %d\r\n", appData.host,appData.port);            
            appData.queryState = IP_ADDRESS_TYPE_IPV4; //Makes sure IPV4 is used (Hardcoded for now)
            
            //Pre-setup
            appData.dnsComplete = 0;
            appData.connectionOpened = 0;
            appData.sslNegComplete = 0;
            appData.firstRxDataPacket = 0;
            appData.lastRxDataPacket = 0;
            appData.rawBytesReceived = 0;
            appData.rawBytesSent = 0;
            appData.clearBytesReceived = 0;
            appData.clearBytesSent = 0;
            
            //Next App State After Parsing IP Port
            appData.state = APP_TCPIP_OPEN_SECURE_SOCKET;
            break;
        }
        case APP_TCPIP_OPEN_SECURE_SOCKET:
        {
            SYS_CONSOLE_PRINT("Creating a secure socket for %s at port %d\r\n",appData.host,appData.port);
            appData.socket = NET_PRES_SocketOpen(0,
                        NET_PRES_SKT_ENCRYPTED_STREAM_CLIENT,
                        IP_ADDRESS_TYPE_IPV4,
                        appData.port,
                        (NET_PRES_ADDRESS *) & appData.address,
                        NULL);
            
//            NET_PRES_SocketWasReset(appData.socket);
            
            if(appData.socket == NET_PRES_INVALID_SOCKET)
            {
                SYS_CONSOLE_PRINT("Secure Socket Creation Failed \r\n"
                 "NetPres_OpenSocketError Code: %d\r\n", appData.socket);
                appData.state = APP_TCPIP_WAITING_FOR_COMMAND;
            }
            else
            {
                SYS_CONSOLE_PRINT("Secure Socket Created Successfully\r\n"
                        "Connecting Socket....\r\n");
                appData.state = APP_TCPIP_WAIT_FOR_SECURE_CONNECTION;
            }     
            
            break;
        }
        
        case APP_TCPIP_WAIT_FOR_SECURE_CONNECTION:
        {
            if (!NET_PRES_SocketIsConnected(appData.socket)) 
            {
//                SYS_CONSOLE_PRINT("Connect unsuccess\r\n"); //Make this verbose if selected
                break;
            }
            else
            {
                SYS_CONSOLE_MESSAGE("Connection Opened: Starting SSL Negotiation\r\n");
                appData.state = APP_TCPIP_WAIT_FOR_NEGOTIATION;
            }
            break;
        }
        case APP_TCPIP_WAIT_FOR_NEGOTIATION:
        {
            if(NET_PRES_SocketIsNegotiatingEncryption(appData.socket)) 
            {
                break;
            }
            else
            {
                SYS_CONSOLE_MESSAGE("Negotiation Complete\r\n");
            }
            
            if(NET_PRES_SocketIsSecure(appData.socket)) 
            {
                SYS_CONSOLE_PRINT("Socket is secure.\r\n"
                        "Start of encrypted exchange.\r\n\r\n"
                        "   Please use \"send_msg\" to send message to %s\r\n\r\n",appData.ipPortBuffer);
                
                //Create Signal Handler Here which allows asynchronous communications
                appData.receivehandle = NET_PRES_SocketSignalHandlerRegister(appData.socket, (uint16_t)TCPIP_TCP_SIGNAL_RX_DATA , (NET_PRES_SIGNAL_FUNCTION)_APP_MessageReceiveHandler, (void*)(0));
                appData.state = APP_TCPIP_WAIT_FOR_MESSAGE;
                break;
            }
            else
            {
                SYS_CONSOLE_MESSAGE("Could not establish secure connection\r\n");
                appData.state = APP_TCPIP_CLOSE_CONNECTION;
            }
            break;
        }
        case APP_TCPIP_WAIT_FOR_MESSAGE:
        {
            //Waits for send_msg command
            
            break;
        }
        case APP_TCPIP_SEND_MESSAGE:
        {
            if(NET_PRES_SocketWriteIsReady(appData.socket,sizeof(appData.message),1))
            {
                NET_PRES_SocketWrite(appData.socket, appData.message,sizeof(appData.message));
                SYS_CONSOLE_PRINT("The message, \"%s\" ,was written to socket\r\n",appData.message);
                appData.state = APP_TCPIP_WAIT_FOR_MESSAGE;
                break;
            }
            else
            {
                SYS_CONSOLE_MESSAGE("Not enough space for this message\r\n");
            }
            break;
        }
        case APP_TCPIP_RECV_MESSAGE:
        {
            /* This state is called asynchronously */
            
            if(NET_PRES_SocketReadIsReady(appData.socket) == 0)
            {
                //Stay in this state until Read is Ready
                break;
            }
            
            //Some kind of benchmark?
            if (appData.firstRxDataPacket == 0) {
                appData.firstRxDataPacket = SYS_TMR_SystemCountGet();
            }
            appData.lastRxDataPacket = SYS_TMR_SystemCountGet();
            
            //Clear networkBuffer
            memset(networkBuffer, 0, sizeof (networkBuffer));
            
            //Read RX Buffer/FIFO
            uint16_t res = NET_PRES_SocketRead(appData.socket, (uint8_t*) networkBuffer, sizeof (networkBuffer));

            appData.clearBytesReceived += res;
            appData.rawBytesReceived += res;

            SYS_CONSOLE_MESSAGE("Server Response: \r\n");
            SYS_CONSOLE_PRINT("%s\r\n",networkBuffer);
            
            //Return to Old State
            appData.state = appData.oldstate;
            
            break;
        }

        case APP_TCPIP_CLOSE_CONNECTION:
        {
            //Close Receive Handler
            NET_PRES_SocketSignalHandlerDeregister(appData.socket, appData.receivehandle);
            
            //Close Socket
            NET_PRES_SocketClose(appData.socket);
            
            SYS_CONSOLE_MESSAGE("Connection Closed\r\n");
            appData.state = APP_TCPIP_WAITING_FOR_COMMAND;
            break;
        }

        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}

//My custom IP:PORT Parser
int32_t _APP_ParseIPPort(char *ipPort, char **host, TCP_PORT *port)
{
    char *pport;
    
    //Pointer to host field
    *host = strtok(ipPort,":");
    
    //Store pointer for further checking
    pport = strtok(NULL,":");
    
    //Add check if there is string after ':'
    if(pport != NULL)
    {
        *port = atoi(pport);
    }
    else
    {
        *port = 0;
        return APP_NO_PORT;
    }
    
    return APP_NO_ERROR;
}
char* _APP_ParseMessage (char* message)
{
    return message;
}
/* Handler below handles receive signal event from TCPIP TCP Transport Layer */
NET_PRES_SIGNAL_FUNCTION _APP_MessageReceiveHandler(void)
{
    NET_PRES_SIGNAL_HANDLE srecv = 0;       //not sure usage
    
    appData.oldstate = appData.state;       //Save oldstate before receive message state
    
    appData.state = APP_TCPIP_RECV_MESSAGE;
    return srecv; //Still not sure use of return value
}
/*******************************************************************************
 End of File
 */

