#include "javaGD.h"
#include "jGDtalk.h"
#include <Rversion.h>

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

/* the internal representation of a color in this API is RGBa with a=0 meaning transparent and a=255 meaning opaque (hence a means 'opacity'). previous implementation was different (inverse meaning and 0x80 as NA), so watch out. */
#if R_VERSION < 0x20000
#define CONVERT_COLOR(C) ((((C)==0x80000000) || ((C)==-1))?0:(((C)&0xFFFFFF)|((0xFF000000-((C)&0xFF000000)))))
#else
#define CONVERT_COLOR(C) (C)
#endif

static void newXGD_Activate(NewDevDesc *dd);
static void newXGD_Circle(double x, double y, double r,
			  R_GE_gcontext *gc,
			  NewDevDesc *dd);
static void newXGD_Clip(double x0, double x1, double y0, double y1,
			NewDevDesc *dd);
static void newXGD_Close(NewDevDesc *dd);
static void newXGD_Deactivate(NewDevDesc *dd);
static void newXGD_Hold(NewDevDesc *dd);
static Rboolean newXGD_Locator(double *x, double *y, NewDevDesc *dd);
static void newXGD_Line(double x1, double y1, double x2, double y2,
			R_GE_gcontext *gc,
			NewDevDesc *dd);
static void newXGD_MetricInfo(int c, 
			      R_GE_gcontext *gc,
			      double* ascent, double* descent,
			      double* width, NewDevDesc *dd);
static void newXGD_Mode(int mode, NewDevDesc *dd);
static void newXGD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd);
Rboolean newXGD_Open(NewDevDesc *dd, newXGDDesc *xd,
		     char *dsp, double w, double h);
static void newXGD_Polygon(int n, double *x, double *y,
			   R_GE_gcontext *gc,
			   NewDevDesc *dd);
static void newXGD_Polyline(int n, double *x, double *y,
			     R_GE_gcontext *gc,
			     NewDevDesc *dd);
static void newXGD_Rect(double x0, double y0, double x1, double y1,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);
static void newXGD_Size(double *left, double *right,
			 double *bottom, double *top,
			 NewDevDesc *dd);
static double newXGD_StrWidth(char *str, 
			       R_GE_gcontext *gc,
			       NewDevDesc *dd);
static void newXGD_Text(double x, double y, char *str,
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
    jint res;
    
    if (!jvm) { // we're hoping that the JVM pointer won't change :P we fetch it just once
        res= JNI_GetCreatedJavaVMs(&jvm, 1, &l);
        if (res!=0) {
            fprintf(stderr, "JNI_GetCreatedJavaVMs failed! (%d)\n",res); return 0;
        }
        if (l<1) {
            fprintf(stderr, "JNI_GetCreatedJavaVMs said there's no JVM running!\n"); return 0;
        }
    }
    res = (*jvm)->AttachCurrentThread(jvm, &env, 0);
    if (res!=0) {
        fprintf(stderr, "AttachCurrentThread failed! (%d)\n",res); return 0;
    }
    /* if (eenv!=env)
        fprintf(stderr, "Warning! eenv=%x, but env=%x - different environments encountered!\n", eenv, env); */
    return env;
}

#define checkGC(e,xd,gc) sendGC(e,xd,gc,0)

/** check changes in GC and issue corresponding commands if necessary */
static void sendGC(JNIEnv *env, newXGDDesc *xd, R_GE_gcontext *gc, int sendAll) {
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
static void sendAllGC(JNIEnv *env, newXGDDesc *xd, R_GE_gcontext *gc) {
    /*
    printf("Basic GC:\n col=%08x\n fill=%08x\n gamma=%f\n lwd=%f\n lty=%08x\n cex=%f\n ps=%f\n lineheight=%f\n fontface=%d\n fantfamily=\"%s\"\n\n",
	 gc->col, gc->fill, gc->gamma, gc->lwd, gc->lty,
	 gc->cex, gc->ps, gc->lineheight, gc->fontface, gc->fontfamily);
     */
    sendGC(env, xd, gc, 1);
}

/*------- the R callbacks begin here ... ------------------------*/

static void newXGD_Activate(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdActivate", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
	chkX(env);
}

static void newXGD_Circle(double x, double y, double r,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;

    checkGC(env,xd, gc);

    mid = (*env)->GetMethodID(env, xd->talkClass, "gdCircle", "(DDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x, y, r);
	chkX(env);
}

static void newXGD_Clip(double x0, double x1, double y0, double y1,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
       
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdClip", "(DDDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x0, x1, y0, y1);
	chkX(env);
}

static void newXGD_Close(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdClose", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
	chkX(env);
}

static void newXGD_Deactivate(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdDeactivate", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
	chkX(env);
}

static void newXGD_Hold(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdHold", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
	chkX(env);
}

static Rboolean newXGD_Locator(double *x, double *y, NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return FALSE;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdLocator", "()[D");
    if (mid) {
        jobject o=(*env)->CallObjectMethod(env, xd->talk, mid);
        if (o) {
            jdouble *ac=(jdouble*)(*env)->GetDoubleArrayElements(env, o, 0);
            if (!ac) return FALSE;
            *x=ac[0]; *y=ac[1];
            (*env)->ReleaseDoubleArrayElements(env, o, ac, 0);
			chkX(env);
            return TRUE;
        }        
    }
	chkX(env);
    
    return FALSE;
}

static void newXGD_Line(double x1, double y1, double x2, double y2,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdLine", "(DDDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x1, y1, x2, y2);
	chkX(env);
}

static void newXGD_MetricInfo(int c,  R_GE_gcontext *gc,  double* ascent, double* descent,  double* width, NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdMetricInfo", "(I)[D");
    if (mid) {
        jobject o=(*env)->CallObjectMethod(env, xd->talk, mid, c);
        if (o) {
            jdouble *ac=(jdouble*)(*env)->GetDoubleArrayElements(env, o, 0);
            if (!ac) return;
            *ascent=ac[0]; *descent=ac[1]; *width=ac[2];
            (*env)->ReleaseDoubleArrayElements(env, o, ac, 0);
        }        
    }
	chkX(env);
}

static void newXGD_Mode(int mode, NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdMode", "(I)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, mode);
	chkX(env);
}

static void newXGD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    int devNr;
    
    if(!env || !xd || !xd->talk) return;
    
    devNr = devNumber((DevDesc*) dd);

    mid = (*env)->GetMethodID(env, xd->talkClass, "gdNewPage", "(I)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, devNr);
	chkX(env);

    /* this is an exception - we send all GC attributes just after the NewPage command */
    sendAllGC(env, xd, gc);
}

Rboolean newXGD_Open(NewDevDesc *dd, newXGDDesc *xd,  char *dsp, double w, double h)
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

static void newXGD_Polygon(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
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

static void newXGD_Polyline(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
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

static void newXGD_Rect(double x0, double y0, double x1, double y1,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdRect", "(DDDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x0, y0, x1, y1);
    else gdWarning("gdRect: can't get mid ");
	chkX(env);
}

static void newXGD_Size(double *left, double *right,  double *bottom, double *top,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdSize", "()[D");
    if (mid) {
        jobject o=(*env)->CallObjectMethod(env, xd->talk, mid);
        if (o) {
            jdouble *ac=(jdouble*)(*env)->GetDoubleArrayElements(env, o, 0);
            if (!ac) { gdWarning("gdSize: cant's get double*"); return; }
            *left=ac[0]; *right=ac[1]; *bottom=ac[2]; *top=ac[3];
            (*env)->ReleaseDoubleArrayElements(env, o, ac, 0);
        } else gdWarning("gdSize: gdSize returned null");
    }
    else gdWarning("gdSize: can't get mid ");
	chkX(env);
}

static double newXGD_StrWidth(char *str,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    jstring s;
    
    if(!env || !xd || !xd->talk) return;
    
    checkGC(env,xd, gc);
    
    s = (*env)->NewStringUTF(env, str);
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdStrWidth", "(Ljava/lang/String;)D");
    if (mid) return (*env)->CallDoubleMethod(env, xd->talk, mid, s);
    // s not released!
	chkX(env);
    return 0.0;
}

static void newXGD_Text(double x, double y, char *str,  double rot, double hadj,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
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
void setupXGDfunctions(NewDevDesc *dd) {
    dd->open = newXGD_Open;
    dd->close = newXGD_Close;
    dd->activate = newXGD_Activate;
    dd->deactivate = newXGD_Deactivate;
    dd->size = newXGD_Size;
    dd->newPage = newXGD_NewPage;
    dd->clip = newXGD_Clip;
    dd->strWidth = newXGD_StrWidth;
    dd->text = newXGD_Text;
    dd->rect = newXGD_Rect;
    dd->circle = newXGD_Circle;
    dd->line = newXGD_Line;
    dd->polyline = newXGD_Polyline;
    dd->polygon = newXGD_Polygon;
    dd->locator = newXGD_Locator;
    dd->mode = newXGD_Mode;
    dd->hold = newXGD_Hold;
    dd->metricInfo = newXGD_MetricInfo;
}

/*--------- Java Initialization -----------*/

#ifdef Win32
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif
#define USER_CLASSPATH "."

static JDK1_1InitArgs vm1_args;

#ifdef JNI_VERSION_1_2 
static JavaVMInitArgs vm2_args;
/* Classpath. */
#define N_JDK_OPTIONS 3
#define VMARGS_TYPE JavaVMInitArgs
static JavaVMOption *vm2_options;
static JavaVMInitArgs *vm_args;
#else  /* So its not JNI_VERSION 1.2 */
static JDK1_1InitArgs vm2_args;
#define VMARGS_TYPE JDK1_1InitArgs
static JDK1_1InitArgs *vm_args;
#endif  /* finished the version 1.1 material */

int initJVM(char *user_classpath) {
    JNIEnv *env;
    jint res;
    char *classpath;
    
    if(!user_classpath)
        /* use the CLASSPATH environment variable as default */
        user_classpath = (char*) getenv("CLASSPATH");
    if(!user_classpath) user_classpath = "";
    
    vm_args = (VMARGS_TYPE *) &vm2_args;
#if defined(JNI_VERSION_1_2)
    vm_args->version = JNI_VERSION_1_2;
    /* printf("JNI 1.2+\n"); */
#else
    /* printf("JNI 1.1\n"); */
    vm_args->version = 0x00010001;
    vm_args->verbose = 1;
#endif
    /* check the version: if 1.2 not available compute a class path */
    if(JNI_GetDefaultJavaVMInitArgs(vm_args) != 0) {
        vm_args = (VMARGS_TYPE *)(&vm1_args);
#if defined(JNI_VERSION_1_1)
        vm_args->version = JNI_VERSION_1_1;
#endif
        vm1_args.classpath = user_classpath;
        if(JNI_GetDefaultJavaVMInitArgs(vm_args) != 0) {
            printf("Neither 1.1x nor 1.2x version of JDK seems supported\n");
            return -1;    
        }
    }
#if defined(JNI_VERSION_1_2)
    else {
        char *interfaceLibraryProperty, *java_lib_path; 
        int i;
        int total_num_properties, propNum = 0;
        
        /* total_num_properties = N_JDK_OPTIONS+n_properties; */
        total_num_properties = N_JDK_OPTIONS;
        
        /*
         if(RequireLibraries) {
             total_num_properties += 2;
         }
         */
        
        vm2_options = (JavaVMOption *) calloc(total_num_properties, sizeof(JavaVMOption));
        vm2_args.version = JNI_VERSION_1_2;
        vm2_args.options = vm2_options;
        vm2_args.ignoreUnrecognized = JNI_TRUE;
        
        classpath = (char*) calloc(strlen("-Djava.class.path=") + strlen(user_classpath)+1, sizeof(char));
        sprintf(classpath, "-Djava.class.path=%s", user_classpath);
        
        vm2_options[propNum++].optionString = classpath;   
        
        /*   print JNI-related messages */
        /* vm2_options[propNum++].optionString = "-verbose:class,jni"; */
        vm2_args.nOptions = propNum;
    }
    /* Create the Java VM */
    res = JNI_CreateJavaVM(&jvm,(void *)&env,(void *)vm_args);
#else
    tmp = (char*)malloc(strlen(user_classpath) +strlen(vm_args->classpath) + 2);
    strcpy(tmp, user_classpath);
    strcat(tmp,":");
    strcat(tmp,vm_args->classpath);
    vm_args->classpath = tmp;
    
#endif
    if (res != 0 || env == NULL) {
        printf("Can't create Java Virtual Machine (res=%d)\n", res);
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

int initJavaGD(newXGDDesc* xd) {
    JNIEnv *env=getJNIEnv();

    if (!jvm) {
        initJVM(jarClassPath);
        env=getJNIEnv();
    }
            
    if (!env) return -1;
    
    {
        jobject o;
        jmethodID mid;
        jclass c=0;
        char *customClass=getenv("JAVAGD_CLASS_NAME");
        if (customClass) c=(*env)->FindClass(env, customClass);
        if (!c) c=(*env)->FindClass(env, "org/rosuda/javaGD/JavaGD");
        if (!c) c=(*env)->FindClass(env, "JavaGD");
        if (!c) { gdWarning("initJavaGD: can't find JavaGD class"); return -2; };
        
        mid=(*env)->GetMethodID(env, c, "<init>", "()V");
        if (!mid) {
            (*env)->DeleteLocalRef(env, c);  
            gdWarning("initJavaGD: can't get mid for JavaGD constructor");
            return -3;
        }
        o=(*env)->NewObject(env, c, mid);
        if (!o) {
            (*env)->DeleteLocalRef(env, c);  
            gdWarning("initJavaGD: can't instantiate JavaGD");
            return -4;
        }
        xd->talk = (*env)->NewGlobalRef(env, o);
        xd->talkClass = (*env)->NewGlobalRef(env, c);
    }
    
    return 0;
}

