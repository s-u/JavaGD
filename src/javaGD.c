/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1997--2003  Robert Gentleman, Ross Ihaka and the
 *			      R Development Core Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define R_JAVAGD 1
#include "javaGD.h"

/********************************************************/
/* If there are resources that are shared by all devices*/
/* of this type, you may wish to make them globals	*/
/* rather than including them in the device-specific	*/
/* parameters structure (especially if they are large !)*/
/********************************************************/

/* XGD Driver Specific parameters
 * with only one copy for all xGD devices */


/*  XGD Device Driver Arguments	:	*/
/*	1) display name			*/
/*	2) width (pixels)		*/
/*	3) height (pixels)		*/
/*	4) host to connect to		*/
/*	5) tcp port to connect to	*/

Rboolean newXGDDeviceDriver(DevDesc *dd,
			    char *disp_name,
			    double width,
			    double height)
{
  newXGDDesc *xd;
  char *fn;

  printf("TD: newXGDDeviceDriver(\"%s\", %f, %f)\n",disp_name,width,height);

  xd = Rf_allocNewXGDDeviceDesc(10);
  if (!newXGD_Open((NewDevDesc*)(dd), xd, disp_name, width, height)) {
    free(xd);
    return FALSE;
  }
  
  Rf_setNewXGDDeviceData((NewDevDesc*)(dd), 0.6, xd);
  
  return TRUE;
}

/**
  This fills the general device structure (dd) with the XGD-specific
  methods/functions. It also specifies the current values of the
  dimensions of the device, and establishes the fonts, line styles, etc.
 */
int
Rf_setNewXGDDeviceData(NewDevDesc *dd, double gamma_fac, newXGDDesc *xd)
{
  printf("Rf_setNewXGDDeviceData\n");
    dd->newDevStruct = 1;

    /*	Set up Data Structures. */
    setupXGDfunctions(dd);

    /* Set required graphics parameters. */

    /* Window Dimensions in Pixels */
    /* Initialise the clipping rect too */

    dd->left = dd->clipLeft = 0;			/* left */
    dd->right = dd->clipRight = xd->windowWidth;	/* right */
    dd->bottom = dd->clipBottom = xd->windowHeight;	/* bottom */
    dd->top = dd->clipTop = 0;			/* top */

    /* Nominal Character Sizes in Pixels */

    dd->cra[0] = 8;
    dd->cra[1] = 8;

    /* Character Addressing Offsets */
    /* These are used to plot a single plotting character */
    /* so that it is exactly over the plotting point */

    dd->xCharOffset = 0.4900;
    dd->yCharOffset = 0.3333;
    dd->yLineBias = 0.1;

    /* Inches per raster unit */

    dd->ipr[0] = 0.01;
    dd->ipr[1] = 0.01;
    dd->asp = 1.0;

    /* Device capabilities */

    dd->canResizePlot = TRUE;
    dd->canChangeFont = TRUE;
    dd->canRotateText = TRUE;
    dd->canResizeText = TRUE;
    dd->canClip = TRUE;
    dd->canHAdj = 2;
    dd->canChangeGamma = FALSE;

    dd->startps = 10;
    dd->startcol = xd->col;
    dd->startfill = xd->fill;
    dd->startlty = LTY_SOLID;
    dd->startfont = 1;
    dd->startgamma = gamma_fac;

    dd->deviceSpecific = (void *) xd;

    dd->displayListOn = TRUE;

    return(TRUE);
}


/**
 This allocates an newXGDDesc instance  and sets its default values.
 */
newXGDDesc * Rf_allocNewXGDDeviceDesc(double ps)
{
    newXGDDesc *xd;
    /* allocate new device description */
    if (!(xd = (newXGDDesc*)calloc(1, sizeof(newXGDDesc))))
	return FALSE;

    /* From here on, if we need to bail out with "error", */
    /* then we must also free(xd). */

    /*	Font will load at first use.  */

    if (ps < 6 || ps > 24) ps = 12;
    xd->fontface = -1;
    xd->fontsize = -1;
    xd->basefontface = 1;
    xd->basefontsize = ps;

    return(xd);
}


typedef Rboolean (*XGDDeviceDriverRoutine)(DevDesc*, char*, 
					    double, double);

static SEXP gcall;

static char *SaveString(SEXP sxp, int offset)
{
    char *s;
    if(!isString(sxp) || length(sxp) <= offset)
	errorcall(gcall, "invalid string argument");
    s = R_alloc(strlen(CHAR(STRING_ELT(sxp, offset)))+1, sizeof(char));
    strcpy(s, CHAR(STRING_ELT(sxp, offset)));
    return s;
}

static DevDesc* 
Rf_addXGDDevice(char *display, double width, double height)
{
    NewDevDesc *dev = NULL;
    GEDevDesc *dd;
    
    char *devname="xGD";

    printf("Rf_addXGDDevice(\"%s\", %f, %f)\n",display,width,height);

    R_CheckDeviceAvailable();
    BEGIN_SUSPEND_INTERRUPTS {
	/* Allocate and initialize the device driver data */
	if (!(dev = (NewDevDesc*)calloc(1, sizeof(NewDevDesc))))
	    return 0;
	/* Do this for early redraw attempts */
	dev->newDevStruct = 1;
	dev->displayList = R_NilValue;
	/* Make sure that this is initialised before a GC can occur.
	 * This (and displayList) get protected during GC
	 */
	dev->savedSnapshot = R_NilValue;
	/* Took out the GInit because MOST of it is setting up
	 * R base graphics parameters.  
	 * This is supposed to happen via addDevice now.
	 */
	if (!newXGDDeviceDriver((DevDesc*)(dev), display, width, height))
	  {
	    free(dev);
	    if (gcall)
	      errorcall(gcall, "unable to start device %s", devname);
	    else
	      fprintf(stderr, "unable to start device %s", devname);
	    return 0;
	  }
	gsetVar(install(".Device"), mkString(devname), R_NilValue);
	dd = GEcreateDevDesc(dev);
	addDevice((DevDesc*) dd);
	GEinitDisplayList(dd);
	printf("XGD> devNum=%d, dd=%x\n", devNumber((DevDesc*) dd), dd);
    } END_SUSPEND_INTERRUPTS;
    
    return((DevDesc*) dd);
}

void resizedXGD(NewDevDesc *dd);

void reloadXGD(int *dn) {
  GEDevDesc *gd=(GEDevDesc*)GetDevice(*dn);
  if (gd) {
    NewDevDesc *dd=gd->dev;
    printf("reloadXGD: dn=%d, dd=%x\n", *dn, dd);
    if (dd) resizedXGD(dd);
  }
}

void javaGDresize(int dev) {
    int ds=NumDevices();
    int i=0;
    if (dev>=0 && dev<ds) { i=dev; ds=dev+1; }
    while (i<ds) {
        GEDevDesc *gd=(GEDevDesc*)GetDevice(i);
        if (gd) {
            NewDevDesc *dd=gd->dev;
            printf("javaGDresize: device=%d, dd=%x\n", i, dd);
            if (dd) {
                printf("dd->size=%x\n", dd->size);
                dd->size(&(dd->left), &(dd->right), &(dd->bottom), &(dd->top), dd);
                GEplayDisplayList(gd);
            }
        }
        i++;
    }
}

void resizedXGD(NewDevDesc *dd) {
  int devNum;
  newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
  //setupXGDfunctions(dd);
  printf("dd->size=%x\n", dd->size);
  dd->size(&(dd->left), &(dd->right), &(dd->bottom), &(dd->top), dd);
  devNum = devNumber((DevDesc*) dd);
  if (devNum > 0)
    GEplayDisplayList((GEDevDesc*) GetDevice(devNum));
}

SEXP newXGD(char **name, double *w, double *h) {
  Rf_addXGDDevice(*name, *w, *h);  

  return R_NilValue;
}
