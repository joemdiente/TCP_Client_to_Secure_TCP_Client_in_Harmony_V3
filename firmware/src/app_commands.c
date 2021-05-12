/*******************************************************************************
  Sample Application

  File Name:
    app_commands.c

  Summary:
    commands for the tcp client demo app.

  Description:
    
 *******************************************************************************/


// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END

#include "tcpip/tcpip.h"
#include "app_commands.h"
#include "app.h"
#include "config.h"
#include <wolfssl/ssl.h>

#if defined(TCPIP_STACK_COMMAND_ENABLE)
char wolfSSLLog[1024] = {0};
int wolfSSLLogSize = 0;

extern APP_DATA appData;

static void _APP_Commands_IPMode(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv);
static void _APP_Commands_Stats(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv);
static void _APP_Commands_GetUnixTime(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv);
static void _APP_Commands_WolfSSLLog(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv);

//My Custom Application Commands
static void _APP_Commands_ConnectTLS(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv);
static void _APP_Commands_DisconnectTLS(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv);
static void _APP_Commands_SendMessage(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv);

static const SYS_CMD_DESCRIPTOR    appCmdTbl[]=
{
    {"ipmode", _APP_Commands_IPMode, ": Change IP Mode"},
    {"stats", _APP_Commands_Stats, ": Statistics"},
    {"unixtime", _APP_Commands_GetUnixTime, ": Unix Time"},
    {"wolfsslLog", _APP_Commands_WolfSSLLog, ": wolfSSL Log"},
    {"connect_tls", _APP_Commands_ConnectTLS, ": Connect to a server securely"},
    {"disconnect_tls", _APP_Commands_DisconnectTLS,": Disconnect to a server"},
    {"send_msg", _APP_Commands_SendMessage,": send message to server"},
};

bool APP_Commands_Init()
{
    if (!SYS_CMD_ADDGRP(appCmdTbl, sizeof(appCmdTbl)/sizeof(*appCmdTbl), "app", ": app commands"))
    {
        SYS_ERROR(SYS_ERROR_ERROR, "Failed to create TCPIP Commands\r\n", 0);
        return false;
    }

    return true;
}

void _APP_Commands_ConnectTLS(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    
    //Unsure
    wolfSSLLog[0] = 0;
    wolfSSLLogSize = 0;

    //"help connect_tls"
    if (argc != 2)
    {
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Usage: connect_tls <ipv4/v6>:<port>\r\n"
                "   Be sure to set correct ipMode (Default: ipv4)\r\n");
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Ex: connect_tls 192.168.0.1:11111\r\n");
    }
    if (appData.state != APP_TCPIP_WAITING_FOR_COMMAND)
    {
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Demo is in the wrong state to take this command");
    }
    
    appData.state = APP_TCPIP_PARSE_IP_PORT;
    strncpy(appData.ipPortBuffer, argv[1], sizeof(appData.ipPortBuffer));
}
void _APP_Commands_DisconnectTLS(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    
    //Unsure
    wolfSSLLog[0] = 0;
    wolfSSLLogSize = 0;

    //"help disconnect_tls"
    if (argc > 2)
    {
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Usage: Disconnect to the currently connected server\r\n");
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Ex: disconnect_tls \r\n");
    }
    
    if (appData.state != APP_TCPIP_WAIT_FOR_MESSAGE)
    {
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Demo is in the wrong state to take this command");
    }
    else
    {
        //Add checking of argument variable IP and port.
        appData.state = APP_TCPIP_CLOSE_CONNECTION;
    }

}
void _APP_Commands_SendMessage(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    
    //Unsure
    wolfSSLLog[0] = 0;
    wolfSSLLogSize = 0;

    if (argc != 2)
    {
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Usage:  Send message to connected server\r\n");
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Ex: send_msg Hello,World \r\n"
                "   Enclosed in quotation marks\r\n");
    }
    
    if (appData.state != APP_TCPIP_WAIT_FOR_MESSAGE)
    {
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   send_msg error: This should be called after connecting to a server successfully via connect_tls\r\n");
    }
    else
    {
        /* Quick Check if variable argument starts and ends in " and */ //Note: Suddenly, " is not parsed. PS: " " is now parse as a single token as of today so no need to check
//        if(argv[1][0] != '"' || argv[1][strlen(argv[1])-1] != '"')
//        { 
//            strncpy(appData.message,argv[1],sizeof(strlen(argv[1])));
//            (*pCmdIO->pCmdApi->msg)(cmdIoParam, "Message not enclosed in quotation marks and/or has spaces(see: help)\r\n");
//        }
//        if (argv[2] == )
//        {
//            
//        }
//        else
        {
            //clear appData.message first
            memset(appData.message, 0, sizeof(appData.message));
            
            strncpy(appData.message,argv[1],strlen(argv[1]));
            
            appData.state = APP_TCPIP_SEND_MESSAGE;
        } 
    }
}

extern APP_DATA appData;

void _APP_Commands_IPMode(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    if (argc != 2)
    {
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Usage: ipmode <ANY|4|6>\r\n");
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, "   Ex: ipmode 6\r\n");
        return;

    }
    appData.ipMode = atoi(argv[1]);
    
}

void _APP_Commands_Stats(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;

    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Raw Bytes Txed: %d\r\n", appData.rawBytesSent);
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Raw Bytes Rxed: %d\r\n", appData.rawBytesReceived);
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Clear Bytes Txed: %d\r\n", appData.clearBytesSent);
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Clear Bytes Rxed: %d\r\n", appData.clearBytesReceived);

    uint32_t freq = SYS_TMR_SystemCountFrequencyGet();
    uint32_t time = ((appData.dnsComplete - appData.testStart) * 1000ull) / freq;
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "DNS Lookup Time: %d ms\r\n", time);

    time = ((appData.connectionOpened - appData.dnsComplete) * 1000ull) / freq;
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Time to Start TCP Connection: %d ms\r\n", time);

    if (appData.urlBuffer[4] == 's')
    {
        time = ((appData.sslNegComplete - appData.connectionOpened) * 1000ull) / freq;
        (*pCmdIO->pCmdApi->print)(cmdIoParam, "Time to Negotiate SSL Connection: %d ms\r\n", time);

        time = ((appData.firstRxDataPacket - appData.sslNegComplete) * 1000ull) / freq;
        (*pCmdIO->pCmdApi->print)(cmdIoParam, "Time to till first packet from server: %d ms\r\n", time);
    }
    else
    {
        time = ((appData.firstRxDataPacket - appData.connectionOpened) * 1000ull) / freq;
        (*pCmdIO->pCmdApi->print)(cmdIoParam, "Time for first packet from server: %d ms\r\n", time);
    }

    time = ((appData.lastRxDataPacket - appData.firstRxDataPacket) * 1000ull) / freq;
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Time for last packet from server: %d ms\r\n", time);
    
}

void _APP_Commands_GetUnixTime(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    uint32_t sec = TCPIP_SNTP_UTCSecondsGet();
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Time from SNTP: %d\r\n", sec);
    (*pCmdIO->pCmdApi->print)(cmdIoParam, "Low Rez Timer: %d\r\n", SYS_TIME_CounterGet() /
                             SYS_TIME_FrequencyGet());
    
}

static void _APP_Commands_WolfSSLLog(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    SYS_CONSOLE_MESSAGE(wolfSSLLog);
    
}


#endif
