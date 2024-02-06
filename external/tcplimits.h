/*
  (Internal PerformanC's) TCP Limits: Header file providing TCP limits.

  License available on: licenses/performanc.license
*/

/* Lower layers (may) have a lower limit. This is the maximum limit for the TCP packet size. */

#ifndef TCPLIMITS_H
#define TCPLIMITS_H

#ifdef TCPLIMITS_EXPERIMENTAL_SAVE_MEMORY
/* Ethernet limit -- Lower level */
#define TCPLIMITS_PACKET_SIZE 1500
#else
#define TCPLIMITS_PACKET_SIZE 65536
#endif

#endif
