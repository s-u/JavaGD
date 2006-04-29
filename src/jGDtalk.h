#ifndef __JGD_TALK_H__
#define __JGD_TALK_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "javaGD.h"

extern void setupXGDfunctions(NewDevDesc *dd);

Rboolean newXGD_Open(NewDevDesc *dd, newXGDDesc *xd,  char *dsp, double w, double h);


#endif
