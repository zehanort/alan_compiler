/******************************************************************************
 *  CVS version:
 *     $Id: general.c,v 1.1 2004/05/05 22:00:08 nickie Exp $
 ******************************************************************************
 *
 *  C code file : general.c
 *  Project     : PCL Compiler
 *  Version     : 1.0 alpha
 *  Written by  : Nikolaos S. Papaspyrou (nickie@softlab.ntua.gr)
 *  Date        : May 5, 2004
 *  Description : Generic symbol table in C, general variables and functions
 *
 *  Comments: (in Greek iso-8859-7)
 *  ---------
 *  Åèíéêü Ìåôóüâéï Ðïëõôå÷íåßï.
 *  Ó÷ïëÞ Çëåêôñïëüãùí Ìç÷áíéêþí êáé Ìç÷áíéêþí Õðïëïãéóôþí.
 *  ÔïìÝáò Ôå÷íïëïãßáò ÐëçñïöïñéêÞò êáé Õðïëïãéóôþí.
 *  ÅñãáóôÞñéï Ôå÷íïëïãßáò Ëïãéóìéêïý
 */


/* ---------------------------------------------------------------------
   ---------------------------- Header files ---------------------------
   --------------------------------------------------------------------- */

#include <stdlib.h>
#include <string.h>
#include "general.hpp"

/* ---------------------------------------------------------------------
   ----------- Õëïðïßçóç ôùí óõíáñôÞóåùí äéá÷åßñéóçò ìíÞìçò ------------
   --------------------------------------------------------------------- */

void * mynew (size_t size) {
   void * result = malloc(size);
   
   if (result == NULL)
      fatal("\rOut of memory");
   return result;
}

void mydelete (void * p) {
   if (p != NULL)
      free(p);
}

char *stringCopy(char *s) {
  char *result = NULL;
  if (s) {
    result = (char *)mynew((strlen(s) + 1) * sizeof(char));
    strcpy(result, s);
  }
  return result;
}

/* ---------------------------------------------------------------------
   ------- Áñ÷åßï åéóüäïõ ôïõ ìåôáãëùôôéóôÞ êáé áñéèìüò ãñáììÞò --------
   --------------------------------------------------------------------- */

int linecount;
int sem_failed = 0;
