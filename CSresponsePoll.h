/*
 * File: CSresponsePoll.c
 * Created by: Donald Eckels
 * Created on: December 05, 2014
 *
 * This file handles the structure of the response poll circular buffer and
 * its implementation
 */

#ifndef CSRESPONSEPOLL_H
#define	CSRESPONSEPOLL_H
#include <stdint.h>
#include "GenCircleBuffer.h"
#include "CSlink.h"
#include "CScommandParser.h"

typedef enum{
    IMMEDIATE        = 0,
    PENDING          = 1,
    PENDING_COMPLETE = 2,
}response_cmd_type_t;

typedef struct{
    uint32_t epoch;
    uint16_t cmd_ID;
    response_cmd_type_t type :8;
    uint8_t  status;
} resp_poll_t;

typedef struct{
    resp_poll_t poll_queue[66];
    uint8_t     head;
} response_poll_t;

void    initResponsePoll();
uint8_t respPollUserDelete(uint16_t ID);
BOOL respPollSysDelete(uint8_t index);
void respPollEnqueue(resp_poll_t newest);
void respPollUpdatePending(resp_poll_t update);
void respPollAbort(uint8_t status, uint32_t time);
uint16_t respPollResponse(char* telem);
void commandParserResponsePollEnqueue(link_command_t* cmd, link_response_t* response);

#endif	/* CSRESPONSEPOLL_H */

