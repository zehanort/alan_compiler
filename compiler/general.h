/******************************************************************************
 *  CVS version:
 *     $Id: general.h,v 1.1 2004/05/05 22:00:08 nickie Exp $
 ******************************************************************************
 *
 *  C header file : general.h
 *  Project       : PCL Compiler
 *  Version       : 1.0 alpha
 *  Written by    : Nikolaos S. Papaspyrou (nickie@softlab.ntua.gr)
 *  Date          : May 5, 2004
 *  Description   : Generic symbol table in C, general header file
 *
 *  Comments: (in Greek iso-8859-7)
 *  ---------
 *  ������ �������� �����������.
 *  ����� ������������ ��������� ��� ��������� �����������.
 *  ������ ����������� ������������ ��� �����������.
 *  ���������� ����������� ����������
 */


#ifndef __GENERAL_H__
#define __GENERAL_H__

#include <stdlib.h>
#include <string.h>
#include "error.h"

/* ---------------------------------------------------------------------
 * ----------- ��������� ��� ����������� ����������� ������ ------------
 * --------------------------------------------------------------------- */

//void * new    (size_t);
//void   delete (void *);
char *stringCopy(char *s);


/* ---------------------------------------------------------------------
   -------------- ��������� ���������� ��� ������������� ---------------
   --------------------------------------------------------------------- */

extern int linecount;
extern const char * filename;
extern int sem_failed;

#endif
