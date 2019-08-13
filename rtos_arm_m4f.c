// Fahad Syed Karim
// ARM M4F REAL TIME OPERATING SYSTEM

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// 5 Pushbuttons and 5 LEDs, UART
// LEDS on these pins:
// Blue:   PF2 (on-board)
// Red:    PE1
// Green:  PE2
// Yellow: PE3
// Orange: PE4
// PB0:    PA2
// PB1:    PA3
// PB2:    PA4
// PB3:    PA5
// PB4:    PA6

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "tm4c123gh6pm.h"


//Bitbanded Addresses

#define PB0             (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 2*4))) //PA2
#define PB1             (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 3*4))) //PA3
#define PB2             (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 4*4))) //PA4
#define PB3             (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 5*4))) //PA5
#define PB4             (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 6*4))) //PA6

#define RED_LED         (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 1*4))) //PE1
#define GREEN_LED       (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 2*4))) //PE2
#define YELLOW_LED      (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 3*4))) //PE3
#define ORANGE_LED      (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 4*4))) //PE4

#define BLUE_LED        (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4))) //PF2

uint32_t main_stack;

//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// function pointer
typedef void (*_fn)();

// semaphore
#define MAX_SEMAPHORES 5
#define MAX_QUEUE_SIZE 5
struct semaphore
{
    uint16_t count;
    uint16_t queueSize;
    uint32_t processQueue[MAX_QUEUE_SIZE]; // store task index here
    char name[20];
} semaphores[MAX_SEMAPHORES];
uint8_t semaphoreCount = 0;

struct semaphore *keyPressed, *keyReleased, *flashReq, *resource;

// task
#define STATE_INVALID    0 // no task
#define STATE_UNRUN      1 // task has never been run
#define STATE_READY      2 // has run, can resume at any time
#define STATE_DELAYED    3 // has run, but now awaiting timer
#define STATE_BLOCKED    4 // has run, but now blocked by semaphore
#define MAX_TASKS 10       // maximum number of valid tasks

uint8_t taskCurrent = 0;   // index of last dispatched task
uint8_t taskCount = 0;     // total number of valid tasks
uint8_t kill = 0;

//MODES

uint8_t preempt = 0;       //Preemption
uint8_t ps = 1;            // priority scheduling
uint8_t pi = 1;            //priority inheritance

//UART0 Variables

char c;
uint8_t max_chars = 80;
uint8_t count = 0;
uint8_t getout = 0;
char str[81];
uint8_t i;
uint8_t k;
uint8_t j = 0;
uint8_t x = 0;
uint8_t argc;
uint8_t pos[10];
uint8_t arg[10];
char type[10];
char tempbuff;
uint8_t leng;
char str_1[50];
char str_2[50];
char str_3[50];
char str_4[50];
char tempstr[50];
uint32_t result;
uint64_t totaltime;
uint64_t cputotaltime;
uint8_t cputicker;
uint64_t cpuprint[MAX_TASKS];

char namedirectory[MAX_TASKS][20];
uint32_t piddirectory[MAX_TASKS];

bool flip;

uint32_t stack[MAX_TASKS][256];  // 1024 byte stack for each thread

struct _tcb
{
uint8_t state;                 // see STATE_ values above
void *pid;                     // used to uniquely identify thread
void *sp;                      // location of stack pointer for thread
int8_t priority;               // -8=highest to 7=lowest
int8_t currentPriority;        // used for priority inheritance
uint32_t ticks;                // ticks until sleep complete
uint8_t skipcounter;             // count of number of skips (priority scheduling)
char name[16];                 // name of task used in ps command
void *semaphore;               // pointer to the semaphore that is blocking the thread
char pidname[16];
uint64_t cpu;
void *backuppid
} tcb[MAX_TASKS];

//-----------------------------------------------------------------------------
// RTOS Kernel Functions
//-----------------------------------------------------------------------------

void rtosInit()
{

uint8_t i;
taskCount = 0;
for (i = 0; i < MAX_TASKS; i++)
{
tcb[i].state = STATE_INVALID;
tcb[i].pid = 0;
}

}

int rtosScheduler()
{

bool ok;
static uint8_t task = 0xFF;
ok = false;

while (!ok)
{
task++;
if (task >= MAX_TASKS)
{
        task = 0;
}

if(tcb[task].state == STATE_INVALID)
{
        ok = 0;
}

else
{
    if(ps==0)                                                                               //Priority Scheduling OFF
    {
        ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
    }
    else if(ps==1)
    {
        if(tcb[task].skipcounter == 0)
        {
            tcb[task].skipcounter = tcb[task].currentPriority+8;
            ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
        }
        else if(tcb[task].skipcounter != 0)
        {
            tcb[task].skipcounter -=1;
            ok = 0;
        }
    }
}

}
return task;
}



void rtosStart()
{
_fn fn;
taskCurrent = rtosScheduler();
main_stack = getSP();

setSP(tcb[taskCurrent].sp);
tcb[taskCurrent].state = STATE_READY;
fn = tcb[taskCurrent].pid;
fn();

}

uint32_t getSP()
{
__asm("   MOV  R0, SP");
__asm("   BX LR"      );

}

void setSP(uint32_t new_stack)
{
__asm("   MOV  SP, R0");
__asm("   BX LR"      );
}

bool createThread(_fn fn, char name[], int priority)
{
bool ok = false;
uint8_t i = 0;
bool found = false;

if (taskCount < MAX_TASKS)
{
    // make sure fn not already in list (prevent reentrancy)
    while (!found && (i < MAX_TASKS))
    {
        found = (tcb[i++].pid ==  fn);
    }
    if (!found)
    {
        // find first available tcb record
        i = 0;
        while (tcb[i].state != STATE_INVALID) {i++;}
        tcb[i].state = STATE_UNRUN;
        tcb[i].pid = fn;
        tcb[i].backuppid = tcb[i].pid;                                        //Store backup PID for thread-recreation
        tcb[i].sp = &stack[i][255];
        tcb[i].priority = priority;
        tcb[i].currentPriority = priority;
        tcb[i].skipcounter = tcb[i].priority + 8;

        memset(tempstr, NULL, sizeof(tempstr));
        ltoa(tcb[i].pid,tempstr);
//            itoa(i, tcb[i].pidname, 10);
        strcpy(tcb[i].pidname,tempstr);

//            tcb[i].pidname = i +'0';
        strcpy(tcb[i].name,name);

        // increment task count
        taskCount++;
        ok = true;
    }
}
// REQUIRED: allow tasks switches again
return ok;
}

//Destroy Thread (using Button)
void destroyThread(_fn fn)
{
    __asm("   SVC #104");

}

//Kill Thread (using Shell)
void killThread(uint32_t kill)
{
    __asm("   SVC #105");
}

//Set Thread Priority
void setThreadPriority(_fn fn, uint8_t priority)
{
for(i=0;i<MAX_TASKS;i++)
{
    if(tcb[i].pid==fn)
    {
        tcb[i].priority = priority;
        tcb[i].currentPriority = priority;
    }
}
yield();
}

struct semaphore* createSemaphore(uint8_t count)
{
struct semaphore *pSemaphore = 0;
if (semaphoreCount < MAX_SEMAPHORES)
{
    pSemaphore = &semaphores[semaphoreCount++];
    pSemaphore->count = count;
}
return pSemaphore;
}

//PendSV Call
void yield()
{
    __asm("   SVC #100");
}

//Sleep Call
void sleep(uint32_t tick)
{
    __asm("   SVC #101");
}

//Wait Call
void wait(struct semaphore *pSemaphore)
{
    __asm("   SVC #102");
}

//Post Call
void post(struct semaphore *pSemaphore)
{
    __asm("   SVC #103");
}

//1ms Timer Interrupt
void systickIsr()
{

cputicker-=1;                                         //CPU Calculation Timer (200ms)
if(cputicker==0)
{

uint8_t i = 0;
WTIMER5_CTL_R &= ~TIMER_CTL_TAEN;                 //Turn off Wide Timer 5
totaltime = 0;                                    //Zero-Out Total Time
cputotaltime = 0;                                 //Zero-Out Total Time

for(i=0;i<MAX_TASKS;i++)                          //CPU Counter Loop
{
totaltime += tcb[i].cpu;                      //Running Tally of Total CPU Time
cpuprint[i] = tcb[i].cpu;                     //CPU value stored in array for calculations before zeroing out
tcb[i].cpu = 0;                               //CPU value zeroed-out
}

cputotaltime=totaltime;                           //Total time value passed to new variable

WTIMER5_TAV_R = 0;                               //Clear Wide Timer 5
WTIMER5_CTL_R |= TIMER_CTL_TAEN;                 //Turn on Wide Timer 5
cputicker = 200;                                 //Reset CPU Calcualtion Timer to 200ms

}

uint8_t i = 0;

for(i=0;i<=8;i++)                                               //Sleep Function
{
   if(tcb[i].state == STATE_DELAYED && tcb[i].ticks!=0)
       {
       tcb[i].ticks -=1;                                        //Decrement ticks if function is still in Sleep Mode
       }
   if(tcb[i].state == STATE_DELAYED && tcb[i].ticks==0)
       {
       tcb[i].state = STATE_READY;                              //Set function to READY state when sleep cycle is completed
       }
 }

if(preempt==1)                                                  //Preemption Condition

{
    NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;                    //Task Switch every 1ms
}

}

//PendSV
void pendSvIsr()
{

__asm(" PUSH {R4,R5,R6,R7}");                                   //PUSH registers (that are not automatically pushed by compiler) into the stack
tcb[taskCurrent].sp = getSP();                                  //Read SP value of task before switching and store into the tcb block
setSP(main_stack);                                              //Set SP to the main function stack
tcb[taskCurrent].cpu += WTIMER5_TAV_R;                          //Store CPU time into the tcb block for that particular task

taskCurrent = rtosScheduler();                                  //Cycle through tasks in RTOS Scheduler to determine next task to be switched

if(tcb[taskCurrent].state == STATE_READY)                       //Processing for READY tasks
{
   WTIMER5_CTL_R &= ~TIMER_CTL_TAEN;                           //Turn off Wide Timer 5 (to halt CPU time collection)
   WTIMER5_TAV_R = 0;                                          //Clear Wide Timer 5 (to allow new task to start collecting time)

   setSP(tcb[taskCurrent].sp);                                 //Set SP to the new task received from RTOS Scheduler
   WTIMER5_CTL_R |= TIMER_CTL_TAEN;                            //Turn on Wide Timer 5
   __asm(" POP {R4,R5,R6,R7}");                                //PUSH registers (that are not automatically pushed by compiler) into the stack of the new task
}

else                                                            //Processing for UNRUN tasks
{

 //Create a new dummy stack
 stack[taskCurrent][255] = 0x41000000;                         //Valid XPSR Value (Thumb Bit = 1)
 stack[taskCurrent][254] = tcb[taskCurrent].pid;               //Set LR value as the PID of the task
 stack[taskCurrent][253] = tcb[taskCurrent].pid;               //Set PC value as the PID of the task
 stack[taskCurrent][252] = 0x12;                               //Arbitrary value for the R12 register
 stack[taskCurrent][251] = 0x3;                                //Arbitrary value for the R3 register
 stack[taskCurrent][250] = 0x2;                                //Arbitrary value for the R2 register
 stack[taskCurrent][249] = 0x1;                                //Arbitrary value for the R1 register
 stack[taskCurrent][248] = 0x0;                                //Arbitrary value for the R0 register
 stack[taskCurrent][247] = 0xFFFFFFF9;                         //Exception Return - signifies the presence of a stack

 //SP is decremented to account for the new stack that was made
 setSP(tcb[taskCurrent].sp-36);

 //Task state is now switched to READY
 tcb[taskCurrent].state = STATE_READY;
}

}

//Service Calls (for operations that require uninterrupted functioning)
void svCallIsr()
{

uint32_t R_0 = getR0();                                 //Numerical Argument passed to R_0
uint8_t i = 0;
struct semaphore *pR_0 = getR0();                       //Pointer Argument passed to pR_0
pR_0=R_0;

switch(svc_iden())
{

//Yield
case 100:

NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;     //Task Switch
break;

//Sleep
case 101:

if(tcb[taskCurrent].state == STATE_INVALID)         //Switch to other task if current task is sleeping
{
    NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;        //Task Switch
}

else                                                //For currently active tasks:
{
      tcb[taskCurrent].ticks = R_0;                 //Record number of required ticks
      tcb[taskCurrent].state = STATE_DELAYED;       //Set state to DELAYED
      NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;      //Task Switch
}
break;


//Wait
case 102:

if(pR_0->count==0)                                          //For resource with no more counts:
{
tcb[taskCurrent].state = STATE_BLOCKED;         //Block the task that is trying to use resource
uint8_t i = 0;

for(i=0;i<MAX_QUEUE_SIZE;i++)                   //Place the blocked task into the resource queue
    {
    if(pR_0->processQueue[i]== 0)               //Place in the queue if it is empty
    {
        pR_0->processQueue[i]= taskCurrent;     //Set the blocked task as the first task in queue (0 considered as empty, since Task0 (Idle) never uses a resource
        pR_0->queueSize++;                      //Increment the size of the queue
        break;                                  //Break from the 'for loop' once the blocked task is placed in the queue
    }
    }
}
else                                             //For resource with counts available:
{
  pR_0->count--;                                //Decrement Resource Count
}

tcb[taskCurrent].semaphore = pR_0;             //Assign semaphore to tcb of task using resource


//Priority Inheritance
if(pi==0)                                                                                   //For PI off:
{
NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;                                                    //Task Switch
}


else if(pi==1)                                                                             //For PI = 1
{
if(pR_0->queueSize>0)                                                                     //Check for a Task in the Queue
{
  uint8_t i = 0;
  for(i=0;i<MAX_TASKS;i++)                                                              //Check for Tasks currently using the resource
  {
      if(pR_0==tcb[i].semaphore)                                                        //After determining the task currently using the resource:
      {
          if(tcb[pR_0->processQueue[0]].priority<tcb[i].priority)                       //and if said task has a lower priority than the task in the queue:
          {
              tcb[i].currentPriority = tcb[pR_0->processQueue[0]].currentPriority;      //Elevate the priority of the task using the resource to the priority of the task in the queue
              break;                                                                    //Break the loop once this condition is met
          }
      }
  }
}
}
NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;                                                      //Task-Switch
break;


//Post
case 103:
  tcb[taskCurrent].semaphore = 0;                                               //Set Task semaphore value to 0
  tcb[taskCurrent].currentPriority =  tcb[taskCurrent].priority;                //Restore priority to base priority
  pR_0->count+=1;                                                               //Free Up a Resource

  if(pR_0->queueSize>0)                                                         //If the queue has a task:
  {
      tcb[pR_0->processQueue[0]].state = STATE_READY;                           //Activate the task on the queue
      tcb[pR_0->processQueue[0]].semaphore = pR_0;                              //Set the semaphore value of task to the resource it is using
      pR_0->processQueue[0] = 0;                                                //Zero-Out the queue

      uint8_t i = 1;
      for(i=1;i<MAX_QUEUE_SIZE;i++)                                             //Shift tasks in the queue to the left
      {
          pR_0->processQueue[i-1] = pR_0->processQueue[i];
      }

      pR_0->processQueue[4] = 0;                                                //Zero-out the last position in the queue
      pR_0->count-=1;                                                           //Consume the Resource
      pR_0->queueSize-=1;                                                       //Queue size is decreased
  }
 NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;                                       //Task Switch
 break;


//Destroy Thread
case 104:

taskCount-=1;                                                       //Decrement the Task Count
for(i=0;i<MAX_TASKS;i++)                                                            //Check for the task being destroyed
{
if(tcb[i].pid==R_0)                                                                 //Once the task is located:
{
/////////////////////////////////////////////////////////////////////////POST SEMAPHORE/////////////////////////////////////////////////////////////////////////////////////
if(tcb[i].semaphore!=0 && tcb[i].state==STATE_READY)                            //If the task is using a semaphore:
{
    tcb[i].semaphore = 0;                                                   //Set Task semaphore value to 0
    tcb[i].currentPriority =  tcb[i].priority;                              //Restore priority to base priority
    pR_0->count+=1;                                                         //Free Up a Resource
    if(pR_0->queueSize>0)                                                   //If the queue has a task:
    {
      tcb[pR_0->processQueue[0]].state = STATE_READY;                       //Activate the task on the queue
      tcb[pR_0->processQueue[0]].semaphore = pR_0;                          //Set the semaphore value of task to the resource it is using
      pR_0->processQueue[0] = 0;                                            //Zero-Out the queue
      uint8_t k = 1;
      for(k=1;k<MAX_QUEUE_SIZE;k++)                                         //Shift tasks in the queue to the left
      {
          pR_0->processQueue[k-1] = pR_0->processQueue[k];
      }
      pR_0->processQueue[4] = 0;                                            //Zero-out the last position in the queue
      pR_0->count-=1;                                                       //Consume the Resource
      pR_0->queueSize-=1;                                                   //Queue size is decreased
    }
  }
/////////////////////////////////////////////////////////////////////END OF POST SEMAPHORE//////////////////////////////////////////////////////////////////////////////////
if(tcb[i].state==STATE_BLOCKED)                                                //Remove task from resource queue
{
    pR_0=tcb[i].semaphore;
    pR_0->queueSize = 0;
}

tcb[i].pid = 0;                                                               //Set PID to 0
tcb[i].state = STATE_INVALID;                                                 //Set task to INVALID
break;                                                                        //Break from for loop
}
}
NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;                                                      //Task Switch
break;


//Kill using PID
case 105:

for(i=0;i<MAX_TASKS;i++)
{
if(R_0==tcb[i].pid)
{
/////////////////////////////////////////////////////////////////////////POST SEMAPHORE/////////////////////////////////////////////////////////////////////////////////////
if(tcb[i].semaphore!=0 && (tcb[i].state==STATE_READY||tcb[i].state==STATE_DELAYED))                  //If the task is using a semaphore:
{
  pR_0=tcb[i].semaphore;                                                                            //Set pR_0 to the semaphore value of the task

  tcb[i].semaphore=0;                                                                               //Set Task semaphore value to 0
  tcb[i].currentPriority =  tcb[i].priority;                                                        //Restore priority to base priority
  pR_0->count+=1;                                                                                   //Free Up a Resource
  if(pR_0->queueSize>0)                                                                             //If the queue has a task:
   {
    tcb[pR_0->processQueue[0]].state = STATE_READY;                                                 //Activate the task on the queue
    tcb[pR_0->processQueue[0]].semaphore = pR_0;                                                    //Set the semaphore value of task to the resource it is using
    pR_0->processQueue[0] = 0;                                                                      //Zero-Out the queue
    uint8_t k = 1;
    for(k=1;k<MAX_QUEUE_SIZE;k++)                                                                   //Shift tasks in the queue to the left
     {
      pR_0->processQueue[k-1] = pR_0->processQueue[k];
     }
    pR_0->processQueue[4] = 0;                                                                      //Zero-out the last position in the queue
    pR_0->count-=1;                                                                                 //Consume the Resource
    pR_0->queueSize-=1;                                                                             //Queue size is decreased
    }
}
/////////////////////////////////////////////////////////////////////END OF POST SEMAPHORE//////////////////////////////////////////////////////////////////////////////////
if(tcb[i].state==STATE_BLOCKED)                                                                     //Remove task from resource queue
{
    pR_0=tcb[i].semaphore;                                                                          //Locate which queue the task is in
    pR_0->queueSize = 0;                                                                            //Set queuesize of that semaphore to 0
}

tcb[i].pid = 0;                                                                                     //Set PID to 0
memset(tcb[i].pidname, 0, sizeof(tcb[i].pidname));                                                  //Clear the PID string
tcb[i].state = STATE_INVALID;                                                                       //Set task to INVALID
taskCount-=1;                                                                                       //Decrement the Task Count
break;
}
}
NVIC_INT_CTRL_R = NVIC_INT_CTRL_PEND_SV;                                                                    //Task Switch
break;

//Print Process Status
case 106:

putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Process Status");
putsUart0("\n\r");
putsUart0("------------");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Name                PID.                 Status                   CPU  %                Priority");
putsUart0("\n\r");
putsUart0("----                ----                --------                  ------                --------");
putsUart0("\n\r");

for(i=0;i<MAX_TASKS;i++)
{
    if(tcb[i].state != STATE_INVALID)
    {
    //Print Name
    putsUart0(tcb[i].name);
    for(j=0;j<(20-(strlen(tcb[i].name)));j++)
        {
          putsUart0(" ");
        }

    //Print PID
    putsUart0(tcb[i].pidname);
    for(j=0;j<(20-(strlen(tcb[i].pidname)));j++)
        {
          putsUart0(" ");
        }

    //Print States
    uint8_t k = 21;
    if(tcb[i].state==STATE_READY)
    {
        putsUart0("READY");
        for(j=0;j<k;j++)
            {
              putsUart0(" ");
            }
    }
    else if(tcb[i].state==STATE_UNRUN)
    {
        putsUart0("UNRUN");
        for(j=0;j<k;j++)
            {
              putsUart0(" ");
            }
    }
    else if(tcb[i].state==STATE_DELAYED)
    {
        putsUart0("DELAYED");
        for(j=0;j<k-2;j++)
            {
              putsUart0(" ");
            }
    }
    else if(tcb[i].state==STATE_BLOCKED)
    {
        putsUart0("BLOCKED");
        for(j=0;j<k-2;j++)
            {
              putsUart0(" ");
            }
    }

    //CPU Calculations
    memset(str_2, NULL, sizeof(str_2));

    cpuprint[i]=(cpuprint[i]*10000);
    cpuprint[i]=cpuprint[i]/cputotaltime;           //Calculate percentage multiplied by 100

    ltoa(cpuprint[i],str_2);                        //Copy CPU% to a buffer

    if(strlen(str_2)==4)                            //Case for 4-digits
    {
    putcUart0(str_2[0]);
    putcUart0(str_2[1]);
    putsUart0(".");
    putcUart0(str_2[2]);
    putcUart0(str_2[3]);
    }

    if(strlen(str_2)==3)                            //Case for 3-digits
    {
    putcUart0('0');
    putcUart0(str_2[0]);
    putsUart0(".");
    putcUart0(str_2[1]);
    putcUart0(str_2[2]);
    }

    if(strlen(str_2)==2)                            //Case for 2-digits
    {
    putcUart0('0');
    putcUart0('0');
    putsUart0(".");
    putcUart0(str_2[0]);
    putcUart0(str_2[1]);
    }

    if(strlen(str_2)==1)                            //Case for 1-digit
    {
    putcUart0('0');
    putcUart0('0');
    putsUart0(".");
    putcUart0('0');
    putcUart0(str_2[0]);
    }

    if(strlen(str_2)==0)                            //Case for 0%
    {
    putcUart0('0');
    putcUart0('0');
    putsUart0(".");
    putcUart0('0');
    putcUart0('0');
    }
    putsUart0("%");

    for(j=0;j<18;j++)                              //Print required number of spaces
    {
             putsUart0(" ");
    }

    //Print Priority
    uint8_t temppriority;

    //For Negative Priorities
    if(tcb[i].currentPriority<0)
    {
        temppriority=tcb[i].currentPriority*-1;
        putcUart0('-');
        putcUart0(temppriority + '0');
    }
    //For Positive Priorities
    else
    {
       putsUart0(" ");
       putcUart0(tcb[i].currentPriority + '0');
    }
    putsUart0("\n\r");
    }
    }
    }
}

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()

{
    //System Clock
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);
    SYSCTL_GPIOHBCTL_R = 0;
    SYSCTL_RCGC2_R = SYSCTL_RCGC2_GPIOF | SYSCTL_RCGC2_GPIOE | SYSCTL_RCGC2_GPIOA ;

    // Configuring Ports
    GPIO_PORTA_PUR_R = 0x7C; //PUSHBUTTONS (PA2-PA6)
    GPIO_PORTA_DEN_R = 0x7C;

    GPIO_PORTE_DIR_R = 0x1E; //RGYO LED
    GPIO_PORTE_DR2R_R = 0x1E;
    GPIO_PORTE_DEN_R = 0x1E;

    GPIO_PORTF_DIR_R = 0x04;  //B LED (ON BOARD)
    GPIO_PORTF_DR2R_R = 0x04;
    GPIO_PORTF_DEN_R = 0x04;

    // UART0 Pins Configuration
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;                                    // Enable UART0
    GPIO_PORTA_DEN_R |= 3;                                                     // Digital Enable - Bits 0 & 1
    GPIO_PORTA_AFSEL_R |= 3;                                                   // Alternate Function Enable - Bits 0 & 1
    GPIO_PORTA_PCTL_R = GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;

    // UART0 Settings : 115200 baud, 8N1 format
    UART0_CTL_R = 0;                                              // Turn off UART0 (for safe programming)
    UART0_CC_R = UART_CC_CS_SYSCLK;                               // Set to System Clock (40MHz)
    UART0_IBRD_R = 21;                                            // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                            // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;              // Configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;  // Enable TX, RX, and Module

    //SysTick
    NVIC_ST_RELOAD_R = 0x9C40;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R = NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_ENABLE;

    //Wide Timer 5
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R5;                                    // Enable Wide-Timer 5
    WTIMER5_CTL_R &= ~TIMER_CTL_TAEN;                                              // Turn-off Counter before Reconfiguring
    WTIMER5_CFG_R = 4;                                                            // Configure as 32-bit counter
    WTIMER5_TAMR_R = TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR;   // Configure for Edge Time Mode, Count Up
    WTIMER5_CTL_R = TIMER_CTL_TAEVENT_POS;                                        // Measure Time from positive-positive edge
    WTIMER5_TAV_R = 0;                                                             // Clear timer before operation
}

// Approximate busy waiting (in units of microseconds), given a 40 MHz system clock
void waitMicrosecond(uint32_t us)
{
                                              // Approx clocks per us
    __asm("WMS_LOOP0:   MOV  R1, #6");          // 1
    __asm("WMS_LOOP1:   SUB  R1, #1");          // 6
    __asm("             CBZ  R1, WMS_DONE1");   // 5+1*3
    __asm("             NOP");                  // 5
    __asm("             B    WMS_LOOP1");       // 5*3
    __asm("WMS_DONE1:   SUB  R0, #1");          // 1
    __asm("             CBZ  R0, WMS_DONE0");   // 1
    __asm("             B    WMS_LOOP0");       // 1*3
    __asm("WMS_DONE0:");                        // ---
                                                // 40 clocks/us + error
}

uint8_t readPbs()
{

uint8_t output=0;
if(PB0==0)
{
output += 1;
}

if(PB1==0)
{
output += 2;
}

if(PB2==0)
{
output += 4;
}

if(PB3==0)
{
output += 8;
}

if(PB4==0)
{
output += 16;
}

return output;
}

//-----------------------------------------------------------------------------
// YOUR UNIQUE CODE
// REQUIRED: add any custom code in this space
//-----------------------------------------------------------------------------

void get_value(void* fun){}

int getR0(void){}

void putcUart0(char c)
{
while (UART0_FR_R & UART_FR_TXFF);
UART0_DR_R = c;
}

void putsUart0(char* str)
{

uint8_t i;
for (i = 0; i < strlen(str); i++)
putcUart0(str[i]);

}

char getcUart0()
{
while (UART0_FR_R & UART_FR_RXFE)
{
    yield();
}
return UART0_DR_R & 0xFF;
}

//SVC Identifier

uint8_t svc_iden(void)
{
__asm(" MOV R0,SP");
__asm(" ADD R0,R0,#56");                            //Offset
__asm(" LDR R0,[R0]");
__asm(" SUB R0,R0,#0x4");
__asm(" LDR R0,[R0]");
__asm(" AND R0,#0x00FF0000");
__asm(" MOV R0,R0,LSR#16");
}

// ------------------------------------------------------------------------------
//  Task functions
// ------------------------------------------------------------------------------

void idle()
{

preempt=1;

while(true)
{
ORANGE_LED = 1;
waitMicrosecond(1000);
ORANGE_LED = 0;
yield();
}

}

void flash4Hz()
{
    while(true)
    {
        GREEN_LED ^= 1;
        sleep(125);
    }
}

void oneshot()
{
    while(true)
    {
        wait(flashReq);
        YELLOW_LED = 1;
        sleep(1000);
        YELLOW_LED = 0;
    }
}

void partOfLengthyFn()
{
    waitMicrosecond(990);
    yield();
}

void lengthyFn()
{
    uint16_t i;
    while(true)
    {
        wait(resource);
        for (i = 0; i < 5000; i++)
        {
            partOfLengthyFn();
        }
        RED_LED ^= 1;
        post(resource);
    }
}

void readKeys()
{
    uint8_t buttons;
    while(true)
    {
        wait(keyReleased);
        buttons = 0;
        while (buttons == 0)
        {
            buttons = readPbs();
            yield();
        }
        post(keyPressed);
        if ((buttons & 1) != 0)
        {
            YELLOW_LED ^= 1;
            RED_LED = 1;
        }
        if ((buttons & 2) != 0)
        {
            post(flashReq);
            RED_LED = 0;
        }

        if ((buttons & 4) != 0)
        {
            createThread(flash4Hz, "Flash4Hz", 0);
        }
        if ((buttons & 8) != 0)
        {
            destroyThread(flash4Hz);
        }
        if ((buttons & 16) != 0)
        {
            setThreadPriority(lengthyFn, 4);
        }
        yield();
    }
}

void debounce()
{
    uint8_t count;
    while(true)
    {
        wait(keyPressed);
        count = 10;
        while (count != 0)
        {
            sleep(10);
            if (readPbs() == 0)
                count--;
            else
                count = 10;
        }
        post(keyReleased);
    }
}

void uncooperative()
{
    while(true)
    {
        while (readPbs() == 8)
        {

        }
        yield();
    }
}

void important()
{
    while(true)
    {
        wait(resource);
        BLUE_LED = 1;
        sleep(1000);
        BLUE_LED = 0;
        post(resource);
    }
}

void shell()
{

uint8_t x;
for(x=0;x<MAX_TASKS;x++)
{
    piddirectory[x]=tcb[x].pid;
}

for(x=0;x<MAX_TASKS;x++)
{
    strcpy(namedirectory[x],tcb[x].name);
}



//Introduction Message
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("*******************************  ARM M4F REAL TIME OPERATING SYSTEM *******************************************");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("List of Commands");
putsUart0("\n\r");
putsUart0("----------------");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Process List                                     ps");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Inter-Process Communication                      ipcs");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Priority Inheritance ON/OFF                      pi on/off");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Round-Robin/Priority Scheduling                  sched rr/priority");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Preemptive/Co-operative RTOS                     rtos preempt/coop");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("PID of Thread                                    pidof <threadname>");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Kill Thread by PID                               kill <pid>");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Reboot                                           reboot");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("RTOS States                                      states");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Help                                             help");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("***************************************************************************************************************************");
putsUart0("\n\r");
putsUart0("\n\r");

//Entering endless loop for shell
while (true)
{

//Reset Shell Variables
getout=0;
count=0;
k=0;
argc=0;
memset(pos, 0, sizeof(pos));

//Reset Shell Strings
memset(str, NULL, sizeof(str));
memset(str_1, NULL, sizeof(str_1));
memset(str_2, NULL, sizeof(str_2));
memset(str_3, NULL, sizeof(str_3));
memset(type, NULL, sizeof(type));

putsUart0("***************************************************************************************************************************");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Command: ");
putsUart0("\n\r");

while(getout==0)
{
c = getcUart0();

if(c==0x08)                     // Backspace Case
{
    if(count > 0)
    {
        count--;
    }
}

else if (c == 0x0D)             // Carriage Return
{
    str[count]=NULL;
    getout=1;
}

else if(c >= 0x20)              // Printability Test
{
    str[count]=tolower(c);
    count++;
}

if(count == max_chars)
{
    getout=1;
}
}

putsUart0(str);
putsUart0("\r\n");
putsUart0("\r\n");

// Input String Filtering
leng = strlen(str);

// Replace Delimiters w/Nulls
for(i=0;i<leng;i++)
{
    if(str[i]<0x30 | (str[i]>0x39 && str[i]<0x41) | (str[i]>0x5A && str[i]<0x61) | str[i]>0x7A )
    {
        if(str[i]!=0x26)
        {
        str[i] = NULL;
        }
    }
}

// Record Positions and Types
for(i=0;i<leng;i++)
{
if(i==0)
{

if(str[i]>0x2F && str[i]<0x3A)                              //Number
{
    pos[k] = i;
    type[k]='n';
    k++;
    argc++;
}
else if((str[i]>0x60 && str[i]<0x7B) || str[i]==0x26)     //Argument
{
    pos[k] = i;
    type[k]='a';
    k++;
    argc++;
}

}

else
{
 if( ((str[i]>0x2F && str[i]<0x3A) && (str[i-1]==0x00)) || ((str[i]>0x2F && str[i]<0x3A) && (str[i-1]>0x60 && str[i-1]<0x7B)) )
 {
     pos[k] = i;
     type[k] = 'n';
     k++;
     argc++;
 }
 else if( ( (str[i]>0x60 && str[i]<0x7B) ) && (str[i-1]==0x00) || ((str[i]>0x60 && str[i]<0x7B) && (str[i-1]>0x2F && str[i-1]<0x3A)) || str[i]==0x26 )
 {
     pos[k] = i;
     type[k] = 'a';
     k++;
     argc++;
 }
}
}

// Record Arguments and Types

strcpy(str_1,&str[pos[0]]);
strcpy(str_2,&str[pos[1]]);
strcpy(str_3,&str[pos[2]]);
strcpy(str_4,&str[pos[3]]);

if((strcmp("pi",&str[pos[0]])==0) && (strcmp("on",&str[pos[1]])==0) && argc==2)             //Priority Inheritance ON
{
    putsUart0("Priority Inheritance ON");
    putsUart0("\n\r");
    pi = 1;
}

else if((strcmp("pi",&str[pos[0]])==0) && (strcmp("off",&str[pos[1]])==0) && argc==2)             //Priority Inheritance OFF
{
    putsUart0("Priority Inheritance OFF");
    putsUart0("\n\r");
    pi = 0;
}

else if((strcmp("sched",&str[pos[0]])==0) && (strcmp("priority",&str[pos[1]])==0) && argc==2)             //Priority Scheduling ON
{
    putsUart0("Priority Scheduling ON");
    putsUart0("\n\r");
    ps = 1;
}

else if((strcmp("sched",&str[pos[0]])==0) && (strcmp("rr",&str[pos[1]])==0) && argc==2)             //Round Robin Scheduling ON
{
    putsUart0("Round Robin Scheduling ON");
    putsUart0("\n\r");
    ps = 0;
}

else if((strcmp("rtos",&str[pos[0]])==0) && (strcmp("preempt",&str[pos[1]])==0) && argc==2)             //Preemption ON
{
    putsUart0("Preemptive RTOS ON");
    putsUart0("\n\r");
    preempt = 1;
}

else if((strcmp("rtos",&str[pos[0]])==0) && (strcmp("coop",&str[pos[1]])==0) && argc==2)             //Cooperative ON
{
    putsUart0("Cooperative RTOS ON");
    putsUart0("\n\r");
    preempt = 0;
}

else if(strcmp("states",&str[pos[0]])==0 && argc==1)             //States
{
    putsUart0("States:");
    putsUart0("\n\r");
    putsUart0("\n\r");

    if(preempt==1)
    {
        putsUart0("Preemption: ON");
        putsUart0("\n\r");
    }

    else if(preempt==0)
    {
        putsUart0("Preemption: OFF");
        putsUart0("\n\r");
    }

    if(ps==1)
    {
        putsUart0("Priority Scheduling: ON");
        putsUart0("\n\r");
    }

    else if(ps==0)
    {
        putsUart0("Priority Scheduling: OFF");
        putsUart0("\n\r");
    }

    if(pi==1)
    {
        putsUart0("Priority Inheritance: ON");
        putsUart0("\n\r");
    }

    else if(pi==0)
    {
        putsUart0("Priority Inheritance: OFF");
        putsUart0("\n\r");
    }
}

//Process List
else if(strcmp("ps",&str[pos[0]])==0 && argc==1)
{
    __asm("   SVC #106");
}

//Kill
else if( (strcmp("kill",&str[pos[0]])==0) && (type[1]='a') && argc==2)         //Kill
{
bool invalid = 1;
uint8_t i=0;
strcpy(str_4,"Shell");

for(i=0;i<MAX_TASKS;i++)
{
    if(strcmp(str_2,tcb[i].pidname)==0)
    {
    invalid = 0;
    strcpy(str_3,tcb[i].name);

    if(strcmp(str_4,tcb[i].name)==0)
    {
        putsUart0("Error: Cannot Kill Shell");
        putsUart0("\n\r");
        invalid = 1;
        break;
    }
    }
}

if(invalid==0)
{
    leng = strlen(str_2);
    result=0;

    for(i=0; i<leng; i++)
    {
        result = result * 10 + (str_2[i] - '0' );
    }
    killThread(result);
    putsUart0("Killed: ");
    putsUart0(str_3);
    putsUart0("\n\r");
}

else
{
    putsUart0("Invalid PID");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("type in 'help' for a list of commands\n\r");
}

}

//PIDOF
else if( (strcmp("pidof",&str[pos[0]])==0) && (type[1]='a') && argc==2)         //PID of
{
uint8_t i = 0;
uint8_t k = 0;

for(i=0;i<MAX_TASKS+1;i++)
{
memset(str_3, NULL, sizeof(str_3));

for(k=0;k<strlen(tcb[i].name);k++)
{
    str_3[k]=tolower(tcb[i].name[k]);
}

if(strcmp(str_2,str_3)==0)
{
    if(tcb[i].state==STATE_INVALID)
    {
        putsUart0("Invalid Process Name");
        putsUart0("\n\r");
        putsUart0("type in 'help' for a list of commands\n\r");
        break;
    }

    putsUart0("PID: ");
    putsUart0(tcb[i].pidname);
    putsUart0("\n\r");
    break;
}

if(i==MAX_TASKS)
{
    putsUart0("Invalid Process Name");
    putsUart0("\n\r");
    putsUart0("type in 'help' for a list of commands\n\r");
}

}
}

//IPCS
else if(strcmp("ipcs",&str[pos[0]])==0 && argc==1)
{
uint8_t i = 0;
uint8_t k = 0;
uint8_t j = 0;
uint32_t addr = 0;
putsUart0("\n\r");
putsUart0("Inter-Process Communication");
putsUart0("\n\r");
putsUart0("---------------------------");
putsUart0("\n\r");
putsUart0("\n\r");
putsUart0("Resource          Count          Current Task               Queue Size            Task In Queue");
putsUart0("\n\r");
putsUart0("--------          -----          ------------               ----------            -------------");
putsUart0("\n\r");


for(i=0;i<MAX_SEMAPHORES-1;i++)
{
//Resource
putsUart0(semaphores[i].name);                          //Print Resource Name
for(j=0;j<(20-(strlen(semaphores[i].name)));j++)        //Add required number of spaces
{
    putsUart0(" ");
}

//Count
ltoa(semaphores[i].count,str_2);
putsUart0(str_2);
//putcUart0(semaphores[i].count+'0');                     //Print Count
for(j=0;j<14-strlen(str_2);j++)                                       //Add required number of spaces
{
    putsUart0(" ");
}

// Current Task
addr = (int)&semaphores[i];
ltoa(addr,str_3);

for(k=0;k<MAX_TASKS;k++)
{

memset(str_2, NULL, sizeof(str_2));
ltoa(tcb[k].semaphore,str_2);

if( (strcmp(str_2,str_3)==0) && (tcb[k].state == STATE_READY || tcb[k].state == STATE_DELAYED) )
{
    putsUart0(tcb[k].name);
    for(j=0;j<(30-(strlen(tcb[k].name)));j++)
    {
        putsUart0(" ");
    }
    break;
}

//Special Case for Oneshot
else if( (strcmp(str_2,str_3)==0) && (tcb[k].state == STATE_BLOCKED) )
{
for(j=0;j<MAX_TASKS;j++)
{
    flip = 0;
    memset(tempstr, NULL, sizeof(tempstr));
    ltoa(tcb[j].semaphore,tempstr);

    if(j!=k && strcmp(str_3,tempstr)==0)
    {
        flip = 1;
        break;
    }
}

if(flip == 0)
{
    putsUart0(tcb[k].name);
    for(j=0;j<(30-(strlen(tcb[k].name)));j++)
    {
        putsUart0(" ");
    }
    break;
}
}
}

bool invalid=1;
for(k=0;k<MAX_TASKS;k++)
{
    if(strcmp(str_2,str_3)==0)
    {
        invalid=0;
    }
}

if(invalid==1)
{
    putsUart0("N/A");
    for(j=0;j<27;j++)
    {
        putsUart0(" ");
    }
}

//Queue Size Print
putcUart0(semaphores[i].queueSize+'0');

for(j=0;j<18;j++)
{
    putsUart0(" ");
}

//Task in Queue Print
if(semaphores[i].queueSize>0)
{
    putsUart0(tcb[semaphores[i].processQueue[0]].name);
}
else
{
    putsUart0("N/A");
}

putsUart0("\n\r");
}
}

//Process Create
else if(str[pos[1]]==0x26 || str[pos[2]]==0x26 || str[pos[3]]==0x26)
{
    uint8_t i=0;
    uint8_t k=0;
    bool wrong=1;

    for(i=0;i<MAX_TASKS;i++)
    {
        memset(str_3, NULL, sizeof(str_3));
        for(k=0;k<strlen(namedirectory[i]);k++)
        {
            str_3[k] = tolower(namedirectory[i][k]);
        }

        if(strcmp(str_1,str_3)==0)
        {
            uint8_t k;
            bool activate = 1;
            for(k=0;k<MAX_TASKS;k++){
            if((tcb[k].state==STATE_READY || tcb[k].state==STATE_DELAYED || tcb[k].state==STATE_BLOCKED) && (strcmp(str_3,tcb[k].name)==0))
            {
                putsUart0("Thread Already Active !");
                putsUart0("\n\r");
                wrong=0;
                activate = 0;
                break;
            }
            }

            if(activate==1)
            {
            createThread(piddirectory[i],namedirectory[i],0);
            wrong=0;
            putsUart0("Process Created: ");
            putsUart0(namedirectory[i]);
            putsUart0("\n\r");
            putsUart0("\n\r");
            break;
            }
        }
    }

    if(wrong==1)
    {
        putsUart0("ERROR!");
        putsUart0("\n\r");
    }
}

//Reboot
else if(strcmp("reboot",&str[pos[0]])==0 && argc==1)
{
    putsUart0("REBOOT!");
    putsUart0("\n\r");
    putsUart0("***************************************************************************************************************************");
    putsUart0("\n\n\n\r");
    waitMicrosecond(5000);
    NVIC_APINT_R  =  NVIC_APINT_SYSRESETREQ | NVIC_APINT_VECTKEY;
}

//Help Statement
else if(strcmp("help",&str[pos[0]])==0 && argc==1)
{
    putsUart0("\n\r");
    putsUart0("List of Commands");
    putsUart0("\n\r");
    putsUart0("----------------");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Process Status                                   ps");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Inter-Process Communication                      ipcs");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Priority Inheritance ON/OFF                      pi on/off");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Round-Robin/Priority Scheduling                  sched rr/priority");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Preemptive/Co-operative RTOS                     rtos preempt/coop");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("PID of Thread                                    pidof <threadname>");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Kill Thread by PID                               kill <pid>");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Reboot                                           reboot");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("RTOS States                                      states");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("Help                                             help");
    putsUart0("\n\r");
    putsUart0("\n\r");
    putsUart0("\n\r");
}

//Error Conditions
else
{
    putsUart0("ERROR!!\n\r");
    putsUart0("type in 'help' for a list of commands\n\r");
}
//Command Ending
putsUart0("\n\r");
putsUart0("***************************************************************************************************************************");
putsUart0("\n\n\r");
//Task Switch
yield();
}
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{

bool ok;

// Initialize hardware
initHw();
rtosInit();

// Power-up flash
GREEN_LED = 1;
waitMicrosecond(250000);
GREEN_LED = 0;
waitMicrosecond(250000);

uint8_t k = 0;

// Initialize semaphores
keyPressed = createSemaphore(1);
strcpy(semaphores[k].name,"keyPressed");
k+=1;

keyReleased = createSemaphore(0);
strcpy(semaphores[k].name,"keyReleased");
k+=1;

flashReq = createSemaphore(5);
strcpy(semaphores[k].name,"flashReq");
k+=1;

resource = createSemaphore(1);
strcpy(semaphores[k].name,"resource");
k+=1;

k=0;


// Add required idle process at lowest priority
ok =  createThread(idle, "Idle", 7);

// Add other processes
ok &= createThread(lengthyFn, "LengthyFn", 4);
ok &= createThread(flash4Hz, "Flash4Hz", 0);
ok &= createThread(oneshot, "OneShot", -4);
ok &= createThread(readKeys, "ReadKeys", 4);
ok &= createThread(debounce, "Debounce", 4);
ok &= createThread(important, "Important", -8);
ok &= createThread(uncooperative, "Uncoop", 2);
ok &= createThread(shell, "Shell", 0);

// Start up RTOS
if (ok)
    rtosStart(); // never returns
else
    RED_LED = 1;

return 0;
}
