/*
 * File:   CSpendingProcess.c
 * Author: Donald Eckels
 *
 * Created on January 4, 2015, 3:25 PM
 */

#include "debug.h"
#include "CSi2c.h"
#include "CSpendingCommand.h"
#include "CSstateStatusMonitoring.h"
#include "Globals.h"
#include "CSbeacon.h"
#include "metal/cpu.h"
#include "csSDCard.h"
#include "csRadio.h"
#include "CSjournal.h"
#include "CSswitchCommands.h"

/**
 * abortSequence
 * @param status - status value that was determined from which exit condition triggered the abort
 * @param time - time that the abort occurrs
 * INFO: clear out the pending command sequence and update all of the pending commands not executed
 *       in the response poll to show that they were aborted
 */
void abortSequence(uint8_t status, uint32_t time){
    G_SET(csSequence.cmd_queue, NULL);
    respPollAbort(status, time);
}


/**
 * checkCond
 * @param evaluating - the condition_t to be evaluated, which contains the index of the sensor
 *                     to be evaluated, the operator type, and the count value to be compared against
 * @return TRUE representing if the condition_t checked was true or not.
 * INFO: This function takes in a condition_t, which is used by both pending commands for their wait conditions
 *       and as the exit conditions.
 *       The sensor ID is used to get the corresponding value from the last telemetry values.
 *       There are TWO special IDs that are not onboard sensor IDs to identify relative and absolute time conditions.
 *       Absolute time uses the current time (encoded in csunSatEpoch) and relative time subtracts the time the last
 *       command was executed from the current time.
 *       The "current" value is compared to the condition value and the boolean result of the comparison is returned.

 */
BOOL checkCond(condition_t evaluating){
    //need to know sensor number being used for TIME value
    uint32_t sensor_val;
    BOOL ret = false;
    //get the most recent sensor value
    if(evaluating.sensor_id == PSENSOR_COMMAND_RELATIVE_TIME){
        dprintf("relative time check - ");
        sensor_val = (csunSatEpoch(getRTC()) - Global->csSequence.lastCmdTime); //should always be a positive value...
    }
    else if (evaluating.sensor_id == PSENSOR_ABSOLUTE_TIME){
        dprintf("time check - ");
        sensor_val = csunSatEpoch(getRTC());
    }
    else{
        dprintf("sensor %d val - ",evaluating.sensor_id);
        sensor_val = Global->csLastTelemetry.reading[evaluating.sensor_id];
    }
    switch(evaluating.comparator){
        case LESS:
            dprintf("%ld < %ld: ", sensor_val, evaluating.value);
            if(sensor_val<evaluating.value){
                dprintf("true! ");
                ret = true;
            }
            break;
        case LESS_EQ:
            dprintf("%ld <= %ld: ", sensor_val, evaluating.value);
            if(sensor_val<=evaluating.value){
                dprintf("true! ");
                ret = true;
            }
            break;
        case EQUAL:
            dprintf("%ld == %ld: ", sensor_val, evaluating.value);
            if(sensor_val==evaluating.value){
                dprintf("true! ");
                ret = true;
            }
            break;
        case GREATER_EQ:
            dprintf("%ld >= %ld: ", sensor_val, evaluating.value);
            if(sensor_val >= evaluating.value){
                dprintf("true! ");
                ret = true;
            }
            break;
        case GREATER:
            dprintf("%ld > %ld: ", sensor_val, evaluating.value);
            if(sensor_val>evaluating.value){
                dprintf("true! ");
                ret = true;
            }
            break;
        //no default since all potential values are already covered
    }
    return ret;
}

/**
 * decodeAndRunPending
 * @param cmd - command that will be executed
 * RETURN: none
 * INFO: Once a command is to be processed it is passed into this function and it is executed.
 *       A switch statement is used in a similar function to the old version of the command parser
 *       to call the command specified by the opcode.
 */
void decodeAndRunPending(seq_command_t cmd){
    int16_t reformatResult;
    BYTE reformatMode = 1;
    journal_t journalTemp;
    
    switch(cmd.opcode){
        case OP_START_SEQUENCE: //start sequence
            dprintf("Start sequence\r\n");
            break;
        case OP_LOAD_RADIO_CONFIGURATION: //load radio config
            //store away a copy of the journal structure
            Journal_GetStruct(&journalTemp);
            journalTemp.radioConfigs = Global->csSequence.configs;
            Journal_SetStruct(&journalTemp);

            //Send a pointer to the radio configuration structure to Radio Code.
            radioConfig(&Global->csSequence.configs);
            dprintf("Radio Configuration\r\n");
            break;
        case OP_RELOAD_RADIO_CONFIGURATION: //reload default radio configs
            //Send null to radio, indicating to load defualt values
            dprintf("Reload Radio Configuration\r\n");
            Journal_GetStruct(&journalTemp);
            radioConfig(&journalTemp.radioConfigs);
            break;
        case OP_SET_SWITCH://set swith
            setPCASwitch(cmd.params.set_switch.pca_id,cmd.params.set_switch.config);
            dprintf("PCA %u configured as %u\r\n",cmd.params.set_switch.pca_id,cmd.params.set_switch.config);
            //cmd.params.set_switch.id;
            break;
        case OP_PROCESSOR_MODE: //processor mode
            setProcMode(cmd.params.set_proc_mode.mode);
            dprintf("Processor mode has been set.\r\n");
            break;
        case OP_CHECK_SD_CARD: //check SD
            checkSDCard();
            dprintf("SD card check complete.\r\n");
            break;
        case OP_REFORMAT_SD: //reformat SD
            reformatResult = FSformat(reformatMode,0,NULL);
            if(!reformatResult){
                dprintf("SD card reformatted successfully\r\n");
                if (!Storage_Init()) {
                    dprintf("Unable to init SD card. Error: %d\n", FSerror());
                }
            }
            else{
                dprintf("SD card reformat had an error: %d\n", FSerror());
            }
            break;
        case OP_END_SEQUENCE://end sequence
            dprintf("End Sequence\r\n");
            beaconMsgUpdateSingle(SOFTWARE_STATE,'C');
            break;
        default: //uh oh
            dprintf("UNKNOWN PENDING COMMAND IN SEQUENCE\r\n");
            break;

    }
}


/**
 * pendingProcess
 * INPUT: none
 * OUTPUT: none
 * INFO: The master function for pending command sequence execution.
 *       This function is called from the pending process state in the state status monitoring
 *       state machine once per second. While this should happen immediately after telemetry is
 *       recorded, and therefore should not interfere with the telemetry interrupt, it is not
 *       occurring within an interrupt and therefore is placed within an uninterrupted block (by adjusting the processor priority)
 *       to prevent any interrupts from happening during it and potentially leaving the satellite in an
 *       indeterminate or unsafe state.
 *
 *       A sequence is only "ready" if there are commands loaded and the ready flag is set to true
 *
 *       Exit conditions that are relative times are changed to absolute time exit conditions since
 *       the time in which the link was closed (the time the sequence starts processing) is not tracked
 *       anywhere within here (It's not quickly accessible because it may or may not be in the response poll that does
 *       not keep track of opcode. The link could've timed out as well).
 *
 *       The next item on the sequence is peeked at (to get a copy) so that we can quickly check the wait conditions after the exit conditions.
 *       Exit conditions are checked first since we do not want to continue to process a sequence if the exit conditions evaluate to TRUE.
 *           If they do evaluate to TRUE, the sequence is aborted and both response poll and the beacon is updated.
 *       Next the conditions of the next item in the sequence are checked. If they are false nothing happens to the sequence.
 *           If it evaluates to TRUE, that item is pulled off of the stack and executed, the response poll for it is updated, and if there are more
 *           commands, then the current time is stored as the last command time (for relative time checks).
 *       Since the pending process state is a special, transitory state in the state status response state machine, it needs to be put back into
 *       its previous mode so it may resume other operations.
 *
 *       Finally, the processor mode is returned to normal so interrupts may occur.
 */
void pendingProcess(){
    
    //major issues could arise if the processing of the line is interrupted, especially due to the link being established
    //Does not take long and occurs immediately after gathering telemetry so should not cause any issues.
    //the contents of this function are as narrow as this window can be placed.
    cpu_priority_t priority = Metal_SetCPUPriority(UNINTERRUPTIBLE_PRIORITY);

    if( (Global->csSequence.cmd_queue.count > 0) && (Global->csSequence.seq_ready_flag != 0) /*&& (Link_GetMode() & ~(LINK_SEQUENCING | LINK_ACTIVE))*/){
        //check the exit conditions and fix them if either is a relative time
        uint32_t time = csunSatEpoch(getRTC());
        dprintf("time = %ld\r\n", time);
        conditions_t exitCheck = Global->csSequence.exit;
        if(exitCheck.left.sensor_id == 254){ //change this to an absolute time value
            dprintf("delta time  of %ld being changed -",exitCheck.left.value);
            exitCheck.left.value += time;
            dprintf("now absolute of %ld\r\n",exitCheck.left.value);
            exitCheck.left.sensor_id = 255;
            G_SET(csSequence.exit, &exitCheck);
        }
        if(exitCheck.op != JUST){//AND or OR is the operator, meaning right comparator is present
            if(exitCheck.right.sensor_id == 254){ //change this to an absolute time value
                exitCheck.right.value += time;
                exitCheck.right.sensor_id = 255;
                G_SET(csSequence.exit, &exitCheck);
            }
        }

        dprintf("Checking pending Command sequence\r\n");
        //going to place EXIT/WAIT checks for pending commands here.
        resp_poll_t updating;

        
        seq_command_t pendingCmd;
        PendCmdQueue_Peek(&Global->csSequence.cmd_queue, &pendingCmd);
        
        BOOL result = false;
        //first check to see if we need to EXIT
        //pull EXIT conditions
        dprintf("Exit condition check - ");
        exitCheck = Global->csSequence.exit;
        //select the appropriate analysis depending on operator
        switch(exitCheck.op){
            case JUST:
                result = checkCond(exitCheck.left);
                updating.status = 1;
                break;
            case AND:
                result = (checkCond(exitCheck.left) && checkCond(exitCheck.right));
                updating.status = 2;
                break;
            case OR:
                result = checkCond(exitCheck.left);
                if(result){
                    updating.status = 3;
                }
                result = result || checkCond(exitCheck.right);
                if(result && updating.status != 3){
                    updating.status = 4;
                }
                else if(result){
                    updating.status = 5;
                }
                break;
                //no default since these three cases are all possible values
        }

        if(result){
            dprintf("ABORTING SEQUENCE!\r\n");
            //if exit conditions met, ABORT SEQUENCE
            resetPayload();
            abortSequence(updating.status, time);
            beaconMsgUpdateSingle(SOFTWARE_STATE,'D');
        }
        else{
            dprintf("Good!\r\nChecking wait conditons - ");
            result = false;
            //then, if we did NOT exit, peek at the next item in the sequence
            //break down the wait conditions and process according to JUST, AND, or OR
            switch(pendingCmd.wait.op){
                case JUST:
                    result = checkCond(pendingCmd.wait.left);
                    updating.status = 1;
                    break;
                case AND:
                    result = (checkCond(pendingCmd.wait.left) && checkCond(pendingCmd.wait.right));
                    updating.status = 2;
                    break;
                case OR:
                    result = checkCond(pendingCmd.wait.left);
                    if(result){
                        updating.status = 3;
                    }
                    result = result || checkCond(pendingCmd.wait.right);
                    if(result && updating.status != 3){
                        updating.status = 4;
                    }
                    else if(result){
                        updating.status = 5;
                    }
                    break;
                    //no default since these three cases are all possible values
            }
                    //check to see if all necessary conditions have been met
            if(result){
                dprintf("Executing next pending command!\r\n");
                //pop & do the next item in the sequence

                PendCmdQueue queueTemp;
                memcpy(&queueTemp, &Global->csSequence.cmd_queue, sizeof(queueTemp)); //read
                PendCmdQueue_Dequeue(&queueTemp, &pendingCmd); //mod
                G_SET(csSequence.cmd_queue, &queueTemp); //write
                decodeAndRunPending(pendingCmd);
                updating.epoch = time;
                updating.cmd_ID = pendingCmd.cmd_id;
                updating.type = PENDING_COMPLETE;
                updating.status = 0;
                respPollUpdatePending(updating);

                //check if another item is waiting to be run
                if(PendCmdQueue_Count(&Global->csSequence.cmd_queue) > 0){
                    //if there is, check to see if the wait condition has a time
                    PendCmdQueue_Peek(&Global->csSequence.cmd_queue, &pendingCmd);
                    if(pendingCmd.wait.left.sensor_id == 254 || (pendingCmd.wait.op != JUST && pendingCmd.wait.right.sensor_id == 254)){
                        //if so, record the time so the delta can be calculated
                        G_SET(csSequence.lastCmdTime,&time);
                    }
                }
                else{
                    dprintf("Wait conditions not satisfied currently\r\n");
                    //if something needs to be done when a sequence is empty, add that code here
                }
            }
        }
    }
    //go back to whatever type of sub state it was in before
    if(Global->csState.statMonState == PENDING_PROCESS){
        statMonStateToPrevious();
    }
    
    //return processor to previous priority mode
    Metal_SetCPUPriority(priority);
}