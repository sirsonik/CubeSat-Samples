/* 
 * File:   CSstateStatusMonitoring.h
 * Author: akaplan
 *
 * Created on September 13, 2013, 10:27 AM
 */

#ifndef CSSTATESTATUSMONITORING_H
#define	CSSTATESTATUSMONITORING_H
#include "CSdefine.h"
#include "CSpendingCommand.h"
#include "Globals.h"

#ifdef	__cplusplus
extern "C" {
#endif

void changeStatMonState(StatMonState newState);
void statMonStateDiagnosticCheck();
void statMonStateAllQuiet();
void statMonStateBeaconOn();
void statMonStateToPrevious();

void statusMonitoringStateMachine();



#ifdef	__cplusplus
}
#endif

#endif	/* CSSTATESTATUSMONITORING_H */

