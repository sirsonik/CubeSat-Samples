/* 
 * File:   CSpendingProcess.h
 * Author: Donald Eckels
 *
 * Created on January 4, 2015, 3:25 PM
 */

#ifndef CSPENDINGPROCESS_H
#define	CSPENDINGPROCESS_H

#include "CSpendingCommand.h"

void abortSequence(uint8_t status, uint32_t time);
BOOL checkCond(condition_t evaluating);
void decodeAndRunPending(seq_command_t cmd);
void pendingProcess();
#endif	/* CSPENDINGPROCESS_H */

