/*
 * File: CSresponsePoll.c
 * Created by: Donald Eckels
 * Created on: December 05, 2014
 *
 * This file handles the structure of the response poll circular buffer and
 * its implementation
 */

#include "Globals.h"
#include "debug.h"
#include "CScommandParser.h"
#include "CSi2c.h"

/*
 * initResponsePoll
 * INPUT: none
 * RETURN: none
 * INFO:
 * Clears the reponse poll stores in Globals
 */
void initResponsePoll(){
    G_SET(csResponsePoll.head, NULL);
}


/*
 *respPollUserDelete
 * INPUT: uint16_t ID - command ID of the item to be deleted (command IDs are uint16_t)
 * RETURN: uint8_t - 0 if the item was deleted successfully, 0xFF if the command ID
 *         was not found, 0xFE if it was found, but cannot be manually deleted due to being
 *         a pending command
 * INFO:
 * Called by on_RESPONSE_POLL_CLEAR in CScommandParser.c when the command ID given is NOT 0xFFFF (the command ID
 * for erasing the entire response poll). Two boolean values are utilized to signify the two
 * conditions that must be met to allow deletion - existance in the respone poll and
 * being deletable.
 * 1. Response poll is checked to see if the command ID matches the ID of any stored commands.
 *  - If found the found boolean is marked as true.
 * 2. If found, the command's type is checked. If it's immediate or pending complete
 *    the delete boolean is set to true. If the pending command is pending, delete
 *    is left as false - don't want to let the ground delete an individual pending command.
 * 3. If the item is able to be deleted, then it is done so and all other items in the
 *    response poll are moved appropriately.
 * 4. The appropriate return value is selected depending upon the conditions that were met during
 *    the function execution.
 *
 */
uint8_t respPollUserDelete(uint16_t ID){
    uint8_t i;
    BOOL found = false;
    BOOL delete = false;
    //find the index with the parcitular command ID
    for(i=0;(i < Global->csResponsePoll.head)&&(!found); i++){
        //once found
        if( Global->csResponsePoll.poll_queue[i].cmd_ID == ID ){
            found = true;
            //verify it's not a pending command waiting to be executed.
            if(Global->csResponsePoll.poll_queue[i].type != PENDING ){
                delete = true;
                i--;//handle shuffle in secondary loop
            }
        }
    }
    //if the item is to be deleted, shuffle down all other records, minus where the new head will be, since its data doesn't matter
    if(delete){
        for(;i<(Global->csResponsePoll.head - 1);i++){
            G_SET(csResponsePoll.poll_queue[i],&(Global->csResponsePoll.poll_queue[i+1]));
        }
        uint8_t newHead = (Global->csResponsePoll.head -1);
        G_SET(csResponsePoll.head, &newHead);
    }
    if(found && delete){
        return 0; //all went well
    }
    else if(found){
        return 0xFE; //item was a pending command, not deleted
    }
    else{
        return 0xFF; //item was not found
    }

}


/*
 *respPollSysDelete
 * INPUT: uint8_t index - index of the response poll item to be deleted
 * RETURN: BOOL - true if an item was in the index and was deleted, false if it did not exist
 * INFO:
 * Used by the system for removing items. This function is used only by the system to either
 * delete immediate commands when the response poll is full or as part of a pending command
 * update process.
 * Starting at the given index, assuming it's a currently valid index, all items after (newer) than
 * it are shifted back by 1 slot and true is returned. If somehow an invalid index is passed, then
 * false is returned.
 */
BOOL respPollSysDelete(uint8_t index){
    uint8_t i;
    //error check to make sure it's a valid index given
    if(index <= Global->csResponsePoll.head){
        for(i=index;i<(Global->csResponsePoll.head - 1);i++){
                G_SET(csResponsePoll.poll_queue[i],&(Global->csResponsePoll.poll_queue[i+1]));
        }
        uint8_t newHead = (Global->csResponsePoll.head -1);
        G_SET(csResponsePoll.head, &newHead);
        return true;
    }
    return false;
}

//add a new item, if the poll is full, will need to find and delete the oldest pending that has not been executed.
/*
 *respPollEnqueue
 * INPUT: resp_poll_t newest - a response poll item.
 * RETURN: none
 * INFO:
 * Adds a response poll item to the queue. If the queue is full then the oldest immediate
 * command, if there are any, is deleted. If there is now room the item is added to the
 * poll. Otherwise nothing happens.
 * the input data is not checked for validity because the additional processing and memory
 * overhead necessary was decided to be unnecessary, though the only real validity check would
 * be to have this function set the time value. Due to the nature of updating pending commands
 * , the limited values of command IDs, and crashes on the ground station, the command ID of
 * an added item may not be the highest number or even sequential.
 * Only immediate commands are overwritten. This is because having immediate commands in this list
 * is considered a "bonus" and the response poll's primary purpose is for giving information regarding
 * sequence commands. Without over-complicating the communicaitons with the ground (which increases the
 * time necessary to complete the command and decreases the number of tasks that can be peroformed on
 * a pass-over), the satellite does not know which completed pending commands the ground station has
 * received statuses form in a response poll command. Therefore this process of deleting ANY pending
 * commands is left to the discression of the ground station. With the size of the response poll it is
 * unlikely for it to become completely full of pending commands unless the ground completely forgets
 * to clear it.
 */
void respPollEnqueue(resp_poll_t newest){
    //check to see if the buffer's full. if so, find the oldest erasable command
    if(Global->csResponsePoll.head == (sizeof(Global->csResponsePoll.poll_queue)/sizeof(Global->csResponsePoll.poll_queue[0]))){
        uint8_t i;
        uint8_t head = Global->csResponsePoll.head;
        //find the first non-pending command
        for(i=0; i<head; i++){
            if(Global->csResponsePoll.poll_queue[i].type == IMMEDIATE){
                respPollSysDelete(i); //delete it
                i = head; //end the loop
            }
        }
    }
    //if there is room enqueue the item.
    if(Global->csResponsePoll.head < (sizeof(Global->csResponsePoll.poll_queue)/sizeof(Global->csResponsePoll.poll_queue[0]))){
        uint8_t head = Global->csResponsePoll.head;
        G_SET(csResponsePoll.poll_queue[head],&newest); //enqueue new item
        head++;
        G_SET(csResponsePoll.head,&head); //bump up the head
    }
}

//update a pending command due to its execution
/*
 * respPollUpdatePending
 * INPUT: resp_poll_t update - item that is the updated version of an item already in the poll
 * RETURN: none
 * INFO:
 * If an item with the same command ID is in the response poll the original copy is removed.
 * The item fed into the function is then added to the poll. This is done this way because
 * the ground station currently has the ability to delete unexecuted pending commands from the
 * response poll if they send the response poll clear command with the clear all ID (0xFFFF). Additionally,
 * this allows for the possibilty that somehow data is missing or corrupted for the original version
 * of a response poll item by just adding the 'update' item to the poll. This also reduces the complexity
 * of the code in order to allow faster execution.
 */
void respPollUpdatePending(resp_poll_t update){
    uint8_t i;
    uint8_t head = Global->csResponsePoll.head;
    //find the pending command to be updated
    for(i=0; i<head; i++){
        if(Global->csResponsePoll.poll_queue[i].cmd_ID == update.cmd_ID){
            //remove the original version due to its changed status
            respPollSysDelete(i);
            i = head;
        }
    }
    //enequeue the executed verison
    respPollEnqueue(update);
}


//when an abort occurs in a sequence all of the pending commands that were NOT executed must be updated to reflect this.
/*
 * respPollAbort
 * INPUT: uint8_t status - status code corresponding to the abort condition (SEE pendingProcess() in CSpendingProcess.c)
 *        uint32_t time - CSUNsatEpoch time value
 * RETURN: none
 * INFO: Updates any PENDING item in the response poll to a PENDING_COMPLETE with a negative of the status given.
 * Because only one sequence can be uploaded at any given time it is unnecessary to check to see if the command IDs
 * are part of the current sequence. Time is passed into the function because it is always called where the time has
 * already been requested and the number of I2C calls needs to be kept to a minimum in order to keep processes as efficient
 * as possible.
 * An abort line is added to the response poll to explicitly show the time where the abort occurred and under which conditions.
 * Afterwards all pending commands are updated to reflect them being aborted at this time.
 */
void respPollAbort(uint8_t status, uint32_t time){
    resp_poll_t abortLine;
    abortLine.epoch = time;
    abortLine.type = PENDING_COMPLETE;
    abortLine.status = 0 - status; //mirror negative values for ABORT cause
    abortLine.cmd_ID = 0xFFFE; //using 0xFFFE for abort
    respPollEnqueue(abortLine);
    uint16_t i;
    //go through each and update each unexecuted pending and update that it was aborted
    for(i=0;i<Global->csResponsePoll.head;i++){
        //if it's an unexecuted pending command
        if((Global->csResponsePoll.poll_queue[i].type == PENDING)&&(Global->csResponsePoll.poll_queue[i].status == 42)){
            abortLine.cmd_ID = Global->csResponsePoll.poll_queue[i].cmd_ID; //set the command ID (abortLine already is set to PENDING_COMPLETE and status 0xFF
            respPollUpdatePending(abortLine);
            i--; //step back i so it checks the same spot again, since the queue was shifted with the delete
        }
    }
}


//pack the telem data with the repsonse poll
/*
 * respPollResponse
 * INPUT: char * telem - character array utilized to send data to the ground
 * RETURN: uint16_t - tells the number of characters utilized
 * (technically  also char * telem, which houses a copy of all of the data from the response poll
 * INFO:
 * Takes each item from the response poll and moves it into the character array. takes each byte of a given
 * response poll item and packs it into the character array that will be sent to the ground. The type of command
 * is not sent since this data is unnecessary and would waste precious telemetry bandwidth - all pending commands
 * that have not been executed have a status of 42, which is not used by any other type of command.
 */
uint16_t respPollResponse(char* telem){
    uint16_t i;
    //iterate through each one
    for(i=0;i<Global->csResponsePoll.head;i++){
        telem[(i*7)+0] = Global->csResponsePoll.poll_queue[i].cmd_ID >> 8; //MSB first
        telem[(i*7)+1] = Global->csResponsePoll.poll_queue[i].cmd_ID;
        telem[(i*7)+2] = Global->csResponsePoll.poll_queue[i].status;
        telem[(i*7)+3] = Global->csResponsePoll.poll_queue[i].epoch >> 24;
        telem[(i*7)+4] = Global->csResponsePoll.poll_queue[i].epoch >> 16;
        telem[(i*7)+5] = Global->csResponsePoll.poll_queue[i].epoch >> 8;
        telem[(i*7)+6] = Global->csResponsePoll.poll_queue[i].epoch;
    }
    //return the number of characters used
    i = (i*7);
    return i;
}
/*
 *commandParserResponsePollEnqueue
 * INPUT: link_command_t* cmd - used to get the comand ID and opcode
 *        link_response_t* response - used to get the status of the command (if an immediate command)
 * OUTPUT: none
 * INFO:
 * Pointers to the structures used in the command parser were used in order to keep the amount of data
 * being copied into the function to a minimum and allow the most versatility if the function needed to be
 * moved around in the command parser. The command ID is stored away, since this has a large number of possible
 * values, it is unlikely for the satellite to receive two commands with the same ID before the buffer is emptied.
 * Time is not obtained during most commands, therefore it is obtained within the function.
 */
void commandParserResponsePollEnqueue(link_command_t* cmd, link_response_t* response){
    resp_poll_t newItem;
    uint32_t timeBuf = csunSatEpoch(getRTC());
    newItem.cmd_ID = cmd->id;
    newItem.epoch = timeBuf;
    newItem.type = IMMEDIATE;
    newItem.status = response->status;
    //if it was any of the pending commands, mark it as such
    if((command_list[cmd->opcode].allowed_modes & LINK_SEQUENCING) || cmd->opcode==OP_START_SEQUENCE){//any of the pending commands
        newItem.type = PENDING;
        newItem.status = 42;
        //if it was an END SEQUENCE store away the proper time. done here so that we only use ONE getRTC call per command processed
        if(cmd->opcode==OP_END_SEQUENCE){
            G_SET(csSequence.lastCmdTime, &timeBuf);
        }
    }
    respPollEnqueue(newItem);
    }