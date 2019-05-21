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

char escapeChar(char *s) {
  switch (s[2]) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case '0':  return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '\"': return '\"';
    case 'x': {
      char hex[2];
      hex[0] = s[3];
      hex[1] = s[4];
      return (char)strtol(hex, NULL, 16);
    }
  }
}

char *escapeString(char *s) {
  int N = strlen(s)+1; // includes trailing '\0'
  char curr;
  char to_escape[5] = {'\'', '\\', '\0', '\0', '\0'};
  char *escaped = (char *)malloc(N*sizeof(char));
  int i = 1;
  int j = 0;
  while (i < N-1) {
    char curr = s[i];
    printf("curr is %c\n", curr);
    /* curr is not the beginning of an escape sequence */
    if (curr != '\\') {
      escaped[j++] = curr;
    }
    /* curr is the beginning of an escape sequence */    
    else {
      curr = s[++i]; // skip '\'
      if (curr == 'x') {
        to_escape[2] = 'x';
        to_escape[3] = s[++i];
        to_escape[4] = s[i];
      }
      else to_escape[2] = s[i];
      escaped[j++] = escapeChar(to_escape);
    }
    i++;
  }
  escaped[j-1] = '\0';
  return escaped;
}

/* ---------------------------------------------------------------------
   ------- Áñ÷åßï åéóüäïõ ôïõ ìåôáãëùôôéóôÞ êáé áñéèìüò ãñáììÞò --------
   --------------------------------------------------------------------- */

int linecount;
int sem_failed = 0;
