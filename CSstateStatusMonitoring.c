#include "CSstateStatusMonitoring.h"
#include "metal/beacon.h"
#include "CSstateAnomaly.h"
#include "CSpowerSaving.h"
#include "Globals.h"
#include "CSpendingCommand.h"
#include "CStelemetry.h"
#include "CSi2c.h"
#include "debug.h"
#include "CSresponsePoll.h"
#include "CSpendingProcess.h"
#include "CStimers.h"
#include "CScubesat.h"
#include "CSbeacon.h"

#include "delay.h"
/*
 * File:   CSstateStatusMonitoring.c
 * Author: Donald Eckels & warren & akaplan
 *
 * Created on  September 13, 2013, 10:27 AM
 *
 * USAGE/DOCUMENTATION:
 *
 * --See CSstateStatusMonitoring.h
 *
 * CSstateStatusMonitoring serves as the state machine that the satellite will
 * be in for ~90% of the time. It primarily serves 3 functions -
 * 1. processing pending command sequences
 * 2. Sending beacon messages
 * 3. Performing a daily diagnostic check
 *
 * This state machine was designed to be primarily timer driven to control when each transition occurs
 * due to the necessity of having the beacon transmit on a regular basis, however, this was not quite possible
 * due to numerous factors. The first being that there are not enough timers to ensure each item occurs at the
 * specified time interval. Even if this was possible due to the disabling of interrupt nesting items may
 * not occur on their desired frequency due to being pushed later. This would not be good for relatively time
 * critical tasks like telemetry recording and sequence execution. Because each task being performed does not have
 * anywhere near the same time interval it would require the utilization of a status monitoring state "tick" variable
 * to keep track of when each item would need to occur, which was considered and discarded as a setup due to being
 * more complicated than this state machine needed.
 * The previous state machine was utilized as a framework to build the newest one. Since three of the four states
 * are mutually exclusive it was decided that these would utilize a single timer to push it to the next state.
 * The task of processing a pending command sequence needed to take precidence and occur over all states and the fourth
 * state was decided to be declared in order to allow this to happen. Because telemetry is collected once each second,
 * the macroscopic time necessary for items to power on/off when switches are set, and the necessity of verifying the state
 * of the satellite after anything has been done, it is not prudent for the sequence to be checked and another item processed
 * more often than once a second. It was considered for sequence processing to be part of the telemetry collection and logging,
 * but that idea was quickly discarded due to the fact that safeguards would need to be processed to make sure that a sequence
 * would NOT be processed while not in state status monitoring and the telemetry interrupt was already considered to be very
 * long and bulky.
 *
 * The first of these is done by being triggered on a regular basis by telemetry processing.
 * Each time telemetry is processed (in the interrupt). it changes this state machine to
 * PENDING_PROCESS.
 *
 * The second is done via timer driven state changes. When the satellite has been in
 * the ALL_QUIET state for 2.5 minutes, the interrupt it triggered and changes the sate to BEACON_ON
 *
 * The third is done by passing through the DIAGNOSTIC_CHECK state and by validating that a diagnostic
 * check has not been done on that day. If it has not and it is the designated time (hour), it is performed.
 */
/* Type and Constant Definitions*/
//static BOOL demo3InitFlag                =        1;

static uint32_t const BEACON_ON_TIME     =    40000u; //beacon is on for 30 seconds
static uint32_t const ALL_QUIET_TIME     =   140000u; //all quiet is for 150 seconds


/*
 * changeStatMonState
 * INPUT: newState - state for the status monitoring state machine to go to
 * OUTPUT: none
 * INFO:
 * Utilized indirectly by timer to move the status monitoring state machine to the
 * next state. Directly each of the following function utilize this function.
 * The current state is written to the previous state slot and the new state that
 * is given is set as the new state.
 */
void changeStatMonState(StatMonState newState){
    G_SET(csState.statMonPrevState,&Global->csState.statMonState);
    G_SET(csState.statMonState,&newState);
}

/*
 * statMonStateDiagnosticCheck
 * INPUT: none
 * OUTPUT: none
 * INFO:
 * Very simple - changes the status monitoring state machine to diagnostic check
 * mode and disables the timer.
 * Utilized by the timer when the end of the beacon on state is complete
 */
void statMonStateDiagnosticCheck(){
    changeStatMonState(DIAGNOSTIC_CHECK);
    G_SET(csState.timerMode,NULL);
}
/*
 * statMonStateAllQuiet
 * INPUT: none
 * OUTPUT: none
 * INFO:
 * Very simple - changes the status monitoring state machine to diagnostic check
 * mode and disables the timer.
 * Called directly at the end of the Diagnostic Check state.
 */
void statMonStateAllQuiet(){
    changeStatMonState(ALL_QUIET);
    G_SET(csState.timerMode,NULL);
}

/*
 *statMonStateBeaconOn
 * INPUT: none
 * OUTPUT: none
 * INFO:
 * Very simple - changes the status monitoring state machine to beacon on mode
 * and disables the timer.
 * Utilized by the timer when the end of the all quiet state is complete
 */
void statMonStateBeaconOn(){
    changeStatMonState(BEACON_ON);
    G_SET(csState.timerMode,NULL);
}

/*
 *statMonStateToPrevious
 * INPUT: none
 * OUTPUT: none
 * INFO:
 * Very simple - changes the status monitoring state machine to its previous mode
 * Called at the very end of the pending process state in order to return to the
 * current main state
 */
void statMonStateToPrevious(){
    StatMonState reverting = Global->csState.statMonPrevState;
    changeStatMonState(reverting);
}


/**
 * Handles the state and actions of the cubesat during normal operation.
 * Considered the "default state" after initialization.
 * state information:
 *  DIAGNOSTIC_CHECK - once a day a diagnostic check will be performed. Otherwise
 *                     nothing happens.
 * ALL_QUET - If the beacon has not been disabled during BEACON ON, disables it
 *            when it is safe to do so and return the antenna to the radio. Otherwise
 *            the satellite is quiet and does nothing special.
 * PENDING_PROCESS - The satellite is toggled over to this state once each second,
 *                   after telemetry processing. If there is a sequence to be
 *                   processed it is handled here. After an individual pass through
 *                   this state this state machine reverts to the state it was in
 *                   previoiusly.
 * BEACON_ON - Beacon is powered on, the beacon string is updated while waiting
 *             for it to fully power up, and then it is updated. The timer is then
 *             utilized to make sure that we stay in this state until the transmission
 *             should be complete because the radio and beacon CANNOT utilize the
 *             antenna at the same time. If the beacon does finish during this state,
 *             turn it off and return the antenna to the radio.
 */
void statusMonitoringStateMachine(){

    //static uint16 statusMonitoringState = 0;

    powerSavingOff();//turns off any power saving options
    switch( Global->csState.statMonState ){
        case DIAGNOSTIC_CHECK : {
            dprintf("Diagnostic check\r\n");
            bool diagResult = true;
#if 0
            uint8_t day = (getRTC()>> 32);
            if(day != Global->csState.diagDay){
                diagResult = CubeSat_SpawnDiagnostician();
                G_SET(csState.diagDay,&day);
            }
#endif
            if(diagResult){
                statMonStateAllQuiet();
            } else { // Error found in diagnostic check.
                MainState new = ANOMALY;
                G_SET(csState.previousState,&Global->csState.mainState);
                G_SET(csState.mainState,&new);
            }
        } break;
        case ALL_QUIET : {
            /*TODO: When radio side code is complete, check to see if the beacon is off
            and the antenna has been returned to the radio. If not, do it if it's
            possible. Otherwise throw anomaly.*/
            //sit in all quiet mode for 150 seconds, then go to Beacon On
            if((Global->csState.timerMode == OFF) && Global->csState.statMonState == ALL_QUIET){
                dprintf("Setting timer in All quiet\r\n");
                
                beaconPowerOff();

                setTimeout(ALL_QUIET_TIME,&statMonStateBeaconOn, STATUS_MONITOR);
            }
            else{
            }
        } break;
        case PENDING_PROCESS: {
            pendingProcess();
        }break;
        case BEACON_ON : {
            if((Global->csState.timerMode == OFF) && Global->csState.statMonState == BEACON_ON){
                dprintf("Setting timer in beacon on\r\n");
                if(Global->csBeacon.beacon_enabled){
                    
                    beaconPowerOn();

                    beaconMsgUpdateTelemetry();
                    dprintf(Global->csBeacon.beaconMsg);
                    dprintf("\r\n");
#if BEACON_OUT
                    beaconSend();
#endif
                }
                setTimeout(BEACON_ON_TIME,&statMonStateDiagnosticCheck, STATUS_MONITOR);
            }
            else{
            //TODO: check to see if the beacon is done and it's OK to return the antenna to the radio.
            }
        } break;
        default : {
            anomalyChange();
        }
    }
}