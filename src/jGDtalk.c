#include "javaGD.h"
#include "jGDtalk.h"
#include <Rdefines.h>

int initJavaGD(newJavaGDDesc* xd);

char *symbol2utf8(const char *c); /* from s2u.c */

/* Device Driver Actions */

#define jgdCheckExceptions chkX

#ifdef JGD_DEBUG
#define gdWarning(S) { printf("[javaGD warning] %s\n", S); jgdCheckExceptions(getJNIEnv()); }
#else
#define gdWarning(S)
#endif

#if R_VERSION < 0x10900
#error This JavaGD needs at least R version 1.9.0
#endif

#if R_VERSION >= R_Version(2,7,0)
#define constxt const
#else
#define constxt
#endif

/* the internal representation of a color in this API is RGBa with a=0 meaning transparent and a=255 meaning opaque (hence a means 'opacity'). previous implementation was different (inverse meaning and 0x80 as NA), so watch out. */
#if R_VERSION < 0x20000
#define CONVERT_COLOR(C) ((((C)==0x80000000) || ((C)==-1))?0:(((C)&0xFFFFFF)|((0xFF000000-((C)&0xFF000000)))))
#else
#define CONVERT_COLOR(C) (C)
#endif

static void newJavaGD_Activate(NewDevDesc *dd);
static void newJavaGD_Circle(double x, double y, double r,
			  R_GE_gcontext *gc,
			  NewDevDesc *dd);
static void newJavaGD_Clip(double x0, double x1, double y0, double y1,
			NewDevDesc *dd);
static void newJavaGD_Close(NewDevDesc *dd);
static void newJavaGD_Deactivate(NewDevDesc *dd);
static void newJavaGD_Hold(NewDevDesc *dd);
static Rboolean newJavaGD_Locator(double *x, double *y, NewDevDesc *dd);
static void newJavaGD_Line(double x1, double y1, double x2, double y2,
			R_GE_gcontext *gc,
			NewDevDesc *dd);
static void newJavaGD_MetricInfo(int c, 
			      R_GE_gcontext *gc,
			      double* ascent, double* descent,
			      double* width, NewDevDesc *dd);
static void newJavaGD_Mode(int mode, NewDevDesc *dd);
static void newJavaGD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd);
Rboolean newJavaGD_Open(NewDevDesc *dd, newJavaGDDesc *xd,
		     char *dsp, double w, double h);
static void newJavaGD_Polygon(int n, double *x, double *y,
			   R_GE_gcontext *gc,
			   NewDevDesc *dd);
static void newJavaGD_Polyline(int n, double *x, double *y,
			     R_GE_gcontext *gc,
			     NewDevDesc *dd);
static void newJavaGD_Rect(double x0, double y0, double x1, double y1,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);
static void newJavaGD_Size(double *left, double *right,
			 double *bottom, double *top,
			 NewDevDesc *dd);
static double newJavaGD_StrWidth(constxt char *str, 
			       R_GE_gcontext *gc,
			       NewDevDesc *dd);
static double newJavaGD_StrWidthUTF8(constxt char *str, 
			       R_GE_gcontext *gc,
			       NewDevDesc *dd);
static void newJavaGD_Text(double x, double y, constxt char *str,
			 double rot, double hadj,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);
static void newJavaGD_TextUTF8(double x, double y, constxt char *str,
			 double rot, double hadj,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);


static R_GE_gcontext lastGC; /** last graphics context. the API send changes, not the entire context, so we cache it for comparison here */

static JavaVM *jvm=0;
char *jarClassPath = ".";


/** check exception for the given environment. The exception is printed only in JGD_DEBUG mode. */
static void chkX(JNIEnv *env)
{
    jthrowable t=(*env)->ExceptionOccurred(env);
    if (t) {
#ifndef JGD_DEBUG
		(*env)->ExceptionDescribe(env);
#endif
        (*env)->ExceptionClear(env);
    }
}

/** get java environment for the current thread or 0 if something goes wrong. */
static JNIEnv *getJNIEnv() {
    JNIEnv *env;
    jsize l;
    jint res = 0;
    
    if (!jvm) { /* we're hoping that the JVM pointer won't change :P we fetch it just once */
        res = JNI_GetCreatedJavaVMs(&jvm, 1, &l);
        if (res != 0) {
	  fprintf(stderr, "JNI_GetCreatedJavaVMs failed! (%d)\n", (int)res); return 0;
        }
        if (l<1) {
	  /* fprintf(stderr, "JNI_GetCreatedJavaVMs said there's no JVM running!\n"); */ return 0;
        }
	if (!jvm)
	  error("Unable to get JVM handle");
    }
    res = (*jvm)->AttachCurrentThread(jvm, (void**) &env, 0);
    if (res!=0) {
        fprintf(stderr, "AttachCurrentThread failed! (%d)\n", (int)res); return 0;
    }
    /* if (eenv!=env)
        fprintf(stderr, "Warning! eenv=%x, but env=%x - different environments encountered!\n", eenv, env); */
    return env;
}

#define checkGC(e,xd,gc) sendGC(e,xd,gc,0)

/** check changes in GC and issue corresponding commands if necessary */
static void sendGC(JNIEnv *env, newJavaGDDesc *xd, R_GE_gcontext *gc, int sendAll) {
    jmethodID mid;
    
    if (sendAll || gc->col != lastGC.col) {
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetColor", "(I)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, CONVERT_COLOR(gc->col));
        else gdWarning("checkGC.gdcSetColor: can't get mid");
		chkX(env);
    }

    if (sendAll || gc->fill != lastGC.fill)  {
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetFill", "(I)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, CONVERT_COLOR(gc->fill));
        else gdWarning("checkGC.gdcSetFill: can't get mid");
		chkX(env);
    }

    if (sendAll || gc->lwd != lastGC.lwd || gc->lty != lastGC.lty) {
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetLine", "(DI)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, gc->lwd, gc->lty);
        else gdWarning("checkGC.gdcSetLine: can't get mid");
		chkX(env);
    }

    if (sendAll || gc->cex!=lastGC.cex || gc->ps!=lastGC.ps || gc->lineheight!=lastGC.lineheight || gc->fontface!=lastGC.fontface || strcmp(gc->fontfamily, lastGC.fontfamily)) {
        jstring s = (*env)->NewStringUTF(env, gc->fontfamily);
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetFont", "(DDDILjava/lang/String;)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, gc->cex, gc->ps, gc->lineheight, gc->fontface, s);
        else gdWarning("checkGC.gdcSetFont: can't get mid");
		chkX(env);
    }
    memcpy(&lastGC, gc, sizeof(lastGC));
}

/* re-set the GC - i.e. send commands for all monitored GC entries */
static void sendAllGC(JNIEnv *env, newJavaGDDesc *xd, R_GE_gcontext *gc) {
    /*
    printf("Basic GC:\n col=%08x\n fill=%08x\n gamma=%f\n lwd=%f\n lty=%08x\n cex=%f\n ps=%f\n lineheight=%f\n fontface=%d\n fantfamily=\"%s\"\n\n",
	 gc->col, gc->fill, gc->gamma, gc->lwd, gc->lty,
	 gc->cex, gc->ps, gc->lineheight, gc->fontface, gc->fontfamily);
     */
    sendGC(env, xd, gc, 1);
}

/*------- the R callbacks begin here ... ------------------------*/

static void newJavaGD_Activate(NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdActivate", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
	chkX(env);
}

static void newJavaGD_Circle(double x, double y, double r,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;

    checkGC(env,xd, gc);

    mid = (*env)->GetMethodID(env, xd->talkClass, "gdCircle", "(DDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x, y, r);
	chkX(env);
}

static void newJavaGD_Clip(double x0, double x1, double y0, double y1,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
       
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdClip", "(DDDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x0, x1, y0, y1);
	chkX(env);
}

static void newJavaGD_Close(NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdClose", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
	chkX(env);
}

static void newJavaGD_Deactivate(NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdDeactivate", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
	chkX(env);
}

static void newJavaGD_Hold(NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;

    if(!env || !xd || !xd->talk) return;

    mid = (*env)->GetMethodID(env, xd->talkClass, "gdHold", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
    chkX(env);
}

static Rboolean newJavaGD_Locator(double *x, double *y, NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return FALSE;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdLocator", "()[D");
    if (mid) {
        jobject o=(*env)->CallObjectMethod(env, xd->talk, mid);
        if (o) {
            jdouble *ac=(jdouble*)(*env)->GetDoubleArrayElements(env, o, 0);
            if (!ac) {
	      (*env)->DeleteLocalRef(env, o);
	      return FALSE;
	    }
            *x=ac[0]; *y=ac[1];
            (*env)->ReleaseDoubleArrayElements(env, o, ac, 0);
	    (*env)->DeleteLocalRef(env, o);
	    chkX(env);
            return TRUE;
        }        
    }
	chkX(env);
    
    return FALSE;
}

static void newJavaGD_Line(double x1, double y1, double x2, double y2,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdLine", "(DDDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x1, y1, x2, y2);
	chkX(env);
}

static void newJavaGD_MetricInfo(int c,  R_GE_gcontext *gc,  double* ascent, double* descent,  double* width, NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    if(c <0) c = -c;
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdMetricInfo", "(I)[D");
    if (mid) {
        jobject o=(*env)->CallObjectMethod(env, xd->talk, mid, c);
        if (o) {
            jdouble *ac=(jdouble*)(*env)->GetDoubleArrayElements(env, o, 0);
            if (!ac) {
	      (*env)->DeleteLocalRef(env, o);
	      return;
	    }
            *ascent=ac[0]; *descent=ac[1]; *width=ac[2];
            (*env)->ReleaseDoubleArrayElements(env, o, ac, 0);
	    (*env)->DeleteLocalRef(env, o);
        }        
    }
	chkX(env);
}

static void newJavaGD_Mode(int mode, NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdMode", "(I)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, mode);
	chkX(env);
}

static void newJavaGD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    int devNr;
    
    if(!env || !xd || !xd->talk) return;
    
    devNr = ndevNumber(dd);

    mid = (*env)->GetMethodID(env, xd->talkClass, "gdNewPage", "(I)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, devNr);
	chkX(env);

    /* this is an exception - we send all GC attributes just after the NewPage command */
    sendAllGC(env, xd, gc);
}

Rboolean newJavaGD_Open(NewDevDesc *dd, newJavaGDDesc *xd,  char *dsp, double w, double h)
{   
    if (initJavaGD(xd)) return FALSE;
    
    xd->fill = 0xffffffff; /* transparent, was R_RGB(255, 255, 255); */
    xd->col = R_RGB(0, 0, 0);
    xd->canvas = R_RGB(255, 255, 255);
    xd->windowWidth = w;
    xd->windowHeight = h;
        
    {
        JNIEnv *env = getJNIEnv();
        jmethodID mid;
        
        if(!env || !xd || !xd->talk) {
            gdWarning("gdOpen: env, xd or talk is null");
            return FALSE;
        }
        
        /* we're not using dsp atm! */
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdOpen", "(DD)V");
        if (mid)
            (*env)->CallVoidMethod(env, xd->talk, mid, w, h);
        else {
            gdWarning("gdOpen: can't get mid");
			chkX(env);
            return FALSE;
        }
		chkX(env);
    }
    
    return TRUE;
}

static jarray newDoubleArray(JNIEnv *env, int n, double *ct)
{
    jdoubleArray da=(*env)->NewDoubleArray(env,n);
    
    if (!da) return 0;
    if (n>0) {
        jdouble *dae;
        dae=(*env)->GetDoubleArrayElements(env, da, 0);
        if (!dae) {
            (*env)->DeleteLocalRef(env,da);
			chkX(env);
            return 0;
        }
        memcpy(dae,ct,sizeof(double)*n);
        (*env)->ReleaseDoubleArrayElements(env, da, dae, 0);
    }
	chkX(env);
    return da;
}

static void newJavaGD_Polygon(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    jarray xa, ya;
    
    if(!env || !xd || !xd->talk) return;

    checkGC(env,xd, gc);

    xa=newDoubleArray(env, n, x);
    if (!xa) return;
    ya=newDoubleArray(env, n, y);
    if (!ya) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdPolygon", "(I[D[D)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, n, xa, ya);
    (*env)->DeleteLocalRef(env, xa); 
    (*env)->DeleteLocalRef(env, ya);
	chkX(env);
}

static void newJavaGD_Polyline(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    jarray xa, ya;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    xa=newDoubleArray(env, n, x);
    if (!xa) return;
    ya=newDoubleArray(env, n, y);
    if (!ya) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdPolyline", "(I[D[D)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, n, xa, ya);
    else gdWarning("gdPolyline: can't get mid ");
    (*env)->DeleteLocalRef(env, xa); 
    (*env)->DeleteLocalRef(env, ya);
	chkX(env);
}

static void newJavaGD_Rect(double x0, double y0, double x1, double y1,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdRect", "(DDDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x0, y0, x1, y1);
    else gdWarning("gdRect: can't get mid ");
	chkX(env);
}

static void newJavaGD_Size(double *left, double *right,  double *bottom, double *top,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdSize", "()[D");
    if (mid) {
        jobject o=(*env)->CallObjectMethod(env, xd->talk, mid);
        if (o) {
            jdouble *ac=(jdouble*)(*env)->GetDoubleArrayElements(env, o, 0);
            if (!ac) {
	      (*env)->DeleteLocalRef(env, o);
	      gdWarning("gdSize: cant's get double*");
	      return;
	    }
            *left=ac[0]; *right=ac[1]; *bottom=ac[2]; *top=ac[3];
            (*env)->ReleaseDoubleArrayElements(env, o, ac, 0);
	    (*env)->DeleteLocalRef(env, o);
        } else gdWarning("gdSize: gdSize returned null");
    }
    else gdWarning("gdSize: can't get mid ");
	chkX(env);
}

static constxt char *convertToUTF8(constxt char *str, R_GE_gcontext *gc)
{
    if (gc->fontface == 5) /* symbol font needs re-coding to UTF-8 */
	str = symbol2utf8(str);
#ifdef translateCharUTF8
    else { /* first check whether we are dealing with non-ASCII at all */
	int ascii = 1;
	constxt unsigned char *c = (constxt unsigned char*) str;
	while (*c) { if (*c > 127) { ascii = 0; break; } c++; }
	if (!ascii) /* non-ASCII, we need to convert it to UTF8 */
	    str = translateCharUTF8(mkCharCE(str, CE_NATIVE));
    }
#endif
    return str;
}

static double newJavaGD_StrWidth(constxt char *str,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    return newJavaGD_StrWidthUTF8(convertToUTF8(str, gc), gc, dd);
}

static double newJavaGD_StrWidthUTF8(constxt char *str,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    jstring s;
    
    if(!env || !xd || !xd->talk) return 0.0;
    
    checkGC(env,xd, gc);
    
    s = (*env)->NewStringUTF(env, str);
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdStrWidth", "(Ljava/lang/String;)D");
    if (mid) return (*env)->CallDoubleMethod(env, xd->talk, mid, s);
    /* s not released! */
	chkX(env);
    return 0.0;
}

static void newJavaGD_Text(double x, double y, constxt char *str,  double rot, double hadj,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGD_TextUTF8(x, y, convertToUTF8(str, gc), rot, hadj, gc, dd);
}

static void newJavaGD_TextUTF8(double x, double y, constxt char *str,  double rot, double hadj,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newJavaGDDesc *xd = (newJavaGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    jstring s;
    
    if(!env || !xd || !xd->talk) return;
        
    checkGC(env,xd, gc);
    
    s = (*env)->NewStringUTF(env, str);
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdText", "(DDLjava/lang/String;DD)V");
    if (mid)
        (*env)->CallVoidMethod(env, xd->talk, mid, x, y, s, rot, hadj);
    (*env)->DeleteLocalRef(env, s);  
	chkX(env);
}

/*-----------------------------------------------------------------------*/

/** fill the R device structure with callback functions */
void setupJavaGDfunctions(NewDevDesc *dd) {
    dd->close = newJavaGD_Close;
    dd->activate = newJavaGD_Activate;
    dd->deactivate = newJavaGD_Deactivate;
    dd->size = newJavaGD_Size;
    dd->newPage = newJavaGD_NewPage;
    dd->clip = newJavaGD_Clip;
    dd->strWidth = newJavaGD_StrWidth;
    dd->text = newJavaGD_Text;
    dd->rect = newJavaGD_Rect;
    dd->circle = newJavaGD_Circle;
    dd->line = newJavaGD_Line;
    dd->polyline = newJavaGD_Polyline;
    dd->polygon = newJavaGD_Polygon;
    dd->locator = newJavaGD_Locator;
    dd->mode = newJavaGD_Mode;
    dd->metricInfo = newJavaGD_MetricInfo;
#if R_GE_version >= 4
    dd->hasTextUTF8 = TRUE;
    dd->strWidthUTF8 = newJavaGD_StrWidthUTF8;
    dd->textUTF8 = newJavaGD_TextUTF8;
#else
    dd->hold = newJavaGD_Hold;
#endif
}

/*--------- Java Initialization -----------*/

#ifdef Win32
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif
#define USER_CLASSPATH "."

#ifdef JNI_VERSION_1_2 
static JavaVMInitArgs vm_args;
static JavaVMOption *vm_options;
#else
#warning "** Java/JNI 1.2 or higher is required **"
** ERROR: Java/JNI 1.2 or higher is required **
/* we can't use #error to signal this on Windows due to a bug in the way dependencies are generated */
#endif

int initJVM(char *user_classpath) {
    JNIEnv *env;
    jint res;
    char *classpath;
    int total_num_properties, propNum = 0;
    
    if(!user_classpath)
        /* use the CLASSPATH environment variable as default */
        user_classpath = (char*) getenv("CLASSPATH");
    if(!user_classpath) user_classpath = "";
    
    vm_args.version = JNI_VERSION_1_2;
    if(JNI_GetDefaultJavaVMInitArgs(&vm_args) != JNI_OK)
      error("Java/JNI 1.2 or higher is required");
        
    total_num_properties = 3; /* leave room for classpath and optional jni debug */
        
    vm_options = (JavaVMOption *) calloc(total_num_properties, sizeof(JavaVMOption));
    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = vm_options;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    
    classpath = (char*) calloc(strlen("-Djava.class.path=") + strlen(user_classpath)+1, sizeof(char));
    sprintf(classpath, "-Djava.class.path=%s", user_classpath);
        
    vm_options[propNum++].optionString = classpath;   
    
    /* vm_options[propNum++].optionString = "-verbose:class,jni"; */
    vm_args.nOptions = propNum;
    /* Create the Java VM */
    res = JNI_CreateJavaVM(&jvm,(void **)&env, &vm_args);

    if (res != 0 || env == NULL) {
      error("Cannot create Java Virtual Machine");
      return -1;
    }
    return 0;
}

/*---------------- R-accessible functions -------------------*/

void setJavaGDClassPath(char **cp) {
    jarClassPath=(char*)malloc(strlen(*cp)+1);
    strcpy(jarClassPath, *cp);
}

void getJavaGDClassPath(char **cp) {
    *cp=jarClassPath;
}

int initJavaGD(newJavaGDDesc* xd) {
    JNIEnv *env=getJNIEnv();

    if (!jvm) {
        initJVM(jarClassPath);
        env=getJNIEnv();
    }
            
    if (!env) return -1;
    
    {
      jobject o = 0;
      int releaseO = 1;
      jmethodID mid;
      jclass c=0;
      char *customClass=getenv("JAVAGD_CLASS_NAME");
      if (!getenv("JAVAGD_USE_RJAVA")) {
	if (customClass) { c=(*env)->FindClass(env, customClass); chkX(env); }
	if (!c) { c=(*env)->FindClass(env, "org/rosuda/javaGD/JavaGD"); chkX(env); }
	if (!c) { c=(*env)->FindClass(env, "JavaGD"); chkX(env); }
      }
      if (!c) {
	/* use rJava to instantiate the JavaGD class */
	SEXP cl;
	int  te;
	if (!customClass || !*customClass) customClass="org/rosuda/javaGD/JavaGD";
	/* require(rJava) to make sure it's loaded */
	cl = R_tryEval(lang2(install("require"), install("rJava")), R_GlobalEnv, &te);
	if (te == 0 && asLogical(cl)) { /* rJava is available and loaded */
	  /* if .jniInitialized is FALSE then no one actually loaded rJava before, so */
	  cl = eval(lang2(install(".jnew"), mkString(customClass)), R_GlobalEnv);
	  chkX(env);
	  if (cl != R_NilValue && inherits(cl, "jobjRef")) {
	    o = (jobject) R_ExternalPtrAddr(GET_SLOT(cl, install("jobj")));
	    releaseO = 0;
	    c = (*env)->GetObjectClass(env, o);
	  }
	}
      }
      if (!c && !o) error("Cannot find JavaGD class.");
      if (!o) {
	mid=(*env)->GetMethodID(env, c, "<init>", "()V");
        if (!mid) {
            (*env)->DeleteLocalRef(env, c);  
            error("Cannot find default JavaGD contructor.");
        }
        o=(*env)->NewObject(env, c, mid);
        if (!o) {
	  (*env)->DeleteLocalRef(env, c);  
	  error("Connot instantiate JavaGD object.");
        }
      }

      xd->talk = (*env)->NewGlobalRef(env, o);
      xd->talkClass = (*env)->NewGlobalRef(env, c);
      (*env)->DeleteLocalRef(env, c);
      if (releaseO) (*env)->DeleteLocalRef(env, o);
    }
    
    return 0;
}

