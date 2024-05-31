* Copy all the files in src directory to the FreeRTOS's src directory

* The Scheduler.h file in src directory contains a few macro definitions for changing the code behavior 
    * schedOVERHEAD           : This macro is used to enable Scheduler overhead
    * schedOVERHEAD_TICKS     : This macro is used to specify the number of ticks the scheduler task runs for as part of scheduler overhead
    * schedSCHEDULING_POLICY  : This macro is used to set the scheduling policy to be used :- 
                                - Set it to schedSCHEDULING_POLICY_RMS for RM Algorithm
                                - Set it to schedSCHEDULING_POLICY_DMS for DM Algorithm
    
    * schedRESOURCE_ACCESS_PROTOCOL : This macro is used to set the resource access protocol to be used :- 
                                      - Set it to schedRESOURCE_ACCESS_PROTOCOL_OCPP for OCPP
                                      - Set it to schedRESOURCE_ACCESS_PROTOCOL_ICPP for ICPP

* The project4.ino file in project4 directory, Compile and Run