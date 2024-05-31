* Copy all the files in src-RM-DM directory to the FreeRTOS's src directory to run RM/DM algorithms
    * The scheduler.h file in src directory contains a few macro definitions for changing the code behavior 
        * schedOVERHEAD           : This macro is used to enable Scheduler overhead
        * schedOVERHEAD_TICKS     : This macro is used to specify the number of ticks the scheduler task runs for as part of scheduler overhead
        * schedSCHEDULING_POLICY  : This macro is used to set the scheduling policy to be used :- 
                                    - Set it to schedSCHEDULING_POLICY_RMS for RM Algorithm
                                    - Set it to schedSCHEDULING_POLICY_DMS for DM Algorithm

* Copy all the files in src-EDF directory to the FreeRTOS's src directory to run EDF algorithms

* The final.ino file in final directory, set TASK_SET to 1 for running Task Set 1 and set it to 2 for running Task Set 2 