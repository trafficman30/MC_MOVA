#if !defined (__CMSTYPES_H)
#define __CMSTYPES_H

/*
    Name:           CmsTypes.h
    Author (Date):  ihenderson, 15/11/05
    Platform:       PC
    Purpose:        Types and defines specific to the Comms modules
					(used generally throughout them).

*/


// Serial and TCP Comms modules can support two connections
// simultaneously - fairly arbitrarily, these are named
// "DATA" and "USER" reflecting the two kinds of communications 
// used in MOVA apps such as PCMOVA, MOVA Comm and MOVA Sim.
typedef enum
{
	CONNECTION_ID_DATA,
	CONNECTION_ID_USER,

	CONNECTION_ID_COUNT

} ConnectionID;


#endif /* __CMSTYPES_H */