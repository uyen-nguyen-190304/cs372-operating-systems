#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants and macro definitions used
 * throughout the Pandos kernel. It includes:
 *  - Hardware & software constants
 *  - Processor status flags
 *  - Exception codes
 *  - Timer & memory constants
 *  - SYSCALL code
 *  - Device interrupt and register definition
 *  - Helper macros for operations and memory alignment
 * 
 ****************************************************************************/



/******************************* Hardware & Software Constants *****************************/

#define PAGESIZE            4096			/* page size in bytes */
#define WORDLEN             4				/* word size in bytes */

#define MAXDEVICES          49              /* maximum number of external devices, plus additional semaphore for pseudo-clock */
#define PCLOCKIDX           MAXDEVICES - 1  /* index of the pseudo-clock */
#define MAXPROC             20              /* Max concurrent processes supported */

#define NUCLEUSSTACKTOP     0x20001000      /* top of the nucleus stack */

/******************************* Processor Status Register Constants *****************************/

#define ALLOFF              0x0                 /* all bits disabled */
#define IECON               0x00000001          /* global interrupt enable */
#define IECOFF              0xFFFFFFFE          /* global interrupt disable */
#define IEPON               0x00000004          /* enable global interrupt after LDST */
#define IMON                0x0000FF00          /* enable all interrupts */
#define PLTON               0x08000000          /* Processor Local Timer enable */
#define USERPON             0x00000008          /* set user-mode after LDST */
#define DIRTYON             0x00000400          /* dirty bit on */
#define VALIDON             0x00000200          /* valid bit on */
#define VALIDOFF            0xFFFFFDFF          /* valid bit off */

/******************************* Timer Constants *****************************/

#define INITIALPLT          5000                /* time slice for scheduler in milliseconds (5ms) */
#define INITIALINTTIMER     100000              /* time slice for system-wide Internal Timer (100ms) */   
#define INFINITE            0xFFFFFFFF          /* infinite time */

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		    0x10000000          /* start address of RAM */
#define RAMBASESIZE		    0x10000004          /* size of RAM */
#define TODLOADDR		    0x1000001C          /* Time of Day (TOD) clock */
#define INTERVALTMR		    0x10000020	        /* interval timer */
#define TIMESCALEADDR	    0x10000024          /* time scale factor */

/******************************* Device Interrupt Constants *****************************/

#define DISKINT			    3                   /* disk interrupt */
#define FLASHINT 		    4                   /* flash memory interrupt */
#define NETWINT 		    5                   /* network interface interrupt */
#define PRNTINT 		    6                   /* printer interrupt */
#define TERMINT			    7                   /* terminal interrupt */

#define DEVREDADDBASE       0x10000054          /* device register base address */
#define DEVINTNUM		    5		            /* interrupt lines used by devices */
#define DEVPERINT		    8		            /* devices per interrupt line */
#define DEVREGLEN		    4		            /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	        16 		            /* device register size in bytes */

/******************************* Device Register Field Numbers *****************************/

/* Non-terminal devices */
#define STATUS			    0                   /* status register  */
#define COMMAND			    1                   /* command register */
#define DATA0			    2                   /* data register 0  */
#define DATA1			    3                   /* data register 1  */

/* Terminal devices */
#define RECVSTATUS  	    0                   /* receiver status     */
#define RECVCOMMAND 	    1                   /* receiver command    */
#define TRANSTATUS  	    2                   /* transmitter status  */
#define TRANCOMMAND 	    3                   /* transmitter command */

/******************************* Device Common Status & Command Codes *****************************/

/* device common STATUS codes */
#define UNINSTALLED		    0                   /* device not installed */
#define READY			    1                   /* device ready */
#define BUSY			    3                   /* device busy */

/* device common COMMAND code */
#define RESET			    0                   /* reset device */
#define ACK				    1                   /* acknowledge device operation */

/******************************* Memory Constants *****************************/

#define KSEG0               0x00000000          /* kernel segment 0 (uncached)*/
#define KSEG1               0x20000000          /* kernel segment 1 (cached) */
#define KSEG2               0x40000000          /* kernel segment 2 */
#define KUSEG               0x80000000          /* user segment */

#define RAMSTART            0x20000000          /* start of ram */
#define BIOSDATAPAGE        0x0FFFF000          /* bios data page */
#define	PASSUPVECTOR	    0x0FFFF900          /* pass up vector page */

/******************************* Exception & Interrupt Constants *****************************/

#define INTCONST            0                   /* interrupt exception code */
#define TLBMIN              1                   /* minimum TLB exception code */
#define TLBMAX              3                   /* maximum TLB exception code */
#define SYSCALLCONST        8                   /* SYSCALL exception code */

#define RESERVEDINSTRUCTION 0xFFFFFF28          /* reserved instruction code */

#define GETEXCEPTIONCODE    0x0000007C          /* mask for the exception code (bits 6..2) */
#define CAUSESHIFT          2                   /* number of bits to shift right to get exception code */

/* Cause register constants for interrupt lines */
#define LINE1INT            0x00000200          /* interrupt line 1 */
#define LINE2INT            0x00000400          /* interrupt line 2 */
#define LINE3INT            0x00000800          /* interrupt line 3 */
#define LINE4INT            0x00001000          /* interrupt line 4 */
#define LINE5INT            0x00002000          /* interrupt line 5 */
#define LINE6INT            0x00004000          /* interrupt line 6 */
#define LINE7INT            0x00008000          /* interrupt line 7 */

/* Interrupt line number */
#define	LINE1			    1				
#define	LINE2			    2			
#define	LINE3			    3			
#define	LINE4			    4			
#define	LINE5			    5			
#define	LINE6			    6			
#define	LINE7			    7			

/* Device interrupt constants */
#define	DEV0INT			    0x00000001		
#define	DEV1INT			    0x00000002		
#define	DEV2INT			    0x00000004		
#define	DEV3INT			    0x00000008		
#define	DEV4INT			    0x00000010		
#define	DEV5INT			    0x00000020		
#define	DEV6INT			    0x00000040		
#define	DEV7INT			    0x00000080		


/* Device number */
#define DEV0                0
#define DEV1                1   
#define DEV2                2
#define DEV3                3
#define DEV4                4
#define DEV5                5
#define DEV6                6
#define DEV7                7        

/* Utility constants */
#define OFFSET              3                   
#define STATUSON            0x0F                /* constants that represents when first four bits in terminal device is on */

/******************************* SYSCALL Constants *****************************/

#define SYS1CALL            1                   /* create process */
#define SYS2CALL            2                   /* terminate process */
#define SYS3CALL            3                   /* passeren (semaphore wait) */
#define SYS4CALL            4                   /* verhogen (semaphore signal) */
#define SYS5CALL            5                   /* wait for IO */
#define SYS6CALL            6                   /* get CPU time */
#define SYS7CALL            7                   /* wait for clock */
#define SYS8CALL            8                   /* get support data */

/******************************* Exception Handling Constants *****************************/

#define	PGFAULTEXCEPT	    0                   /* page fault exception */
#define GENERALEXCEPT	    1                   /* general exception */

/******************************* Utility Constants *****************************/

#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			    static
#define EOS                 '\0'

#ifndef MAXINT
#define MAXINT 		        0xEFFFFFFF
#endif

#define NULL 			    ((void *)0xFFFFFFFF)

/******************************* Helper Macros *****************************/

/* Math operations */
#define	MIN(A,B)		    ((A) < (B) ? A : B)
#define MAX(A,B)		    ((A) < (B) ? B : A)

/* Memory alignment check */
#define	ALIGNED(A)		    (((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

/******************************* Mess to place somewhere *****************************/

#define SWAPPOOLSTART       0x20020000          /* Swap Pool's starting address */
#define SWAPPOOLSIZE        2 * UPROCMAX        /* Swap Pool size */
#define EMPTYFRAME         -1                   /* Empty frame in the Swap Pool */

#define NUMPAGES            32                  /* Number of pages in the process's private page table */
#define VPNMASK             0xFFFFF000          /* Virtual Page Number mask */
#define VPNSHIFT            12                  /* Virtual Page Number shift */
#define TLBMODIFICATION     1                   /* TLB modification exception code */
#define UPROCMAX            8                   /* Number of U-procs to be concurrently executed */
#define MAXSTRINGLENGTH     128                 /* Maximum string length for terminal I/O */
#define STATUSMASK          0xFF                /* Status mask for device status */
#define DEVICEREADY        1                    /* Device ready status for printer */
#define UPROCTEXTSTART     0x80000000          /* U-proc's text segment start address */

#define RECEIVECHAR       2              /* Receive the character from the line */
#define TRANSMITCHAR        2              /* Transmit the character over the line */
#define TERMINALSHIFT       8                 /* Shift for terminal device register */  
#define PRINTCHR            2                   /* Transmit the character in DATA0 over the line */
#define CHARTRANSMITTED     5           /* Character transmitted status for printer */
#define CHARRECEIVED        5           /* Character received status for terminal */
#define EOL                 0x0A            /* End of line character */
#define CHARRECEIVEDSHIFT    8           /* Shift for character received status */

#define FLASHREAD           1             /* Flash read command */
#define FLASHWRITE          2             /* Flash write command */

#define BLOCKSHIFT          8           /* Block shift for flash device */
#define READBLK             2         /* Read block command */
#define WRITEBLK            3         /* Write block command */

#define USERSTACKTOP          0xC0000000          /* User stack top address */
#define ASIDSHIFT   6       /* ASID shift for the process ID */
#define VPNSTART 0x80000        /* Virtual Page Number start address */
#define STACKPAGEVPN 0xBFFFFFFF /* Stack page Virtual Page Number */



#endif  /* CONSTS */
