#include "javaGD.h"
#include "jGDtalk.h"

/* Device Driver Actions */

#ifdef DEBUG
#define gdWarning(S) printf("[javaGD warning] %s", S)
#else
#define gdWarning(S)
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
		     char *dsp, double w, double h, char *host, int port);
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


static R_GE_gcontext lastGC;

static JavaVM *jvm=0;
char *jarClassPath = ".";

static JNIEnv *getJNIEnv() {
    JNIEnv *env;
    jsize l;
    jint res;
    
    if (!jvm) { // we're hoping that the JVM pointer won't change :P we fetch it just once
        res= JNI_GetCreatedJavaVMs(&jvm, 1, &l);
        if (res!=0) {
            fprintf(stderr, "JNI_GetCreatedJavaVMs failed! (%d)\n",res); return;
        }
        if (l<1) {
            fprintf(stderr, "JNI_GetCreatedJavaVMs said there's no JVM running!\n"); return;
        }
    }
    res = (*jvm)->AttachCurrentThread(jvm, &env, 0);
    if (res!=0) {
        fprintf(stderr, "AttachCurrentThread failed! (%d)\n",res); return;
    }
    /* if (eenv!=env)
        fprintf(stderr, "Warning! eenv=%x, but env=%x - different environments encountered!\n", eenv, env); */
    return env;
}

#define checkGC(e,xd,gc) sendGC(e,xd,gc,0)

/* check changes in GC and issue corresponding commands if necessary */
static void sendGC(JNIEnv *env, newXGDDesc *xd, R_GE_gcontext *gc, int sendAll) {
    jmethodID mid;
    
    if (sendAll || gc->col != lastGC.col) {
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetColor", "(I)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, gc->col);
        else gdWarning("checkGC.gdcSetColor: can't get mid");
    }

    if (sendAll || gc->fill != lastGC.fill)  {
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetFill", "(I)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, gc->fill);
        else gdWarning("checkGC.gdcSetFill: can't get mid");
    }

    if (sendAll || gc->lwd != lastGC.lwd || gc->lty != lastGC.lty) {
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetLine", "(DI)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, gc->lwd, gc->lty);
        else gdWarning("checkGC.gdcSetLine: can't get mid");
    }

    if (sendAll || gc->cex!=lastGC.cex || gc->ps!=lastGC.ps || gc->fontface!=lastGC.fontface || strcmp(gc->fontfamily, lastGC.fontfamily)) {
        jstring s = (*env)->NewStringUTF(env, gc->fontfamily);
        mid = (*env)->GetMethodID(env, xd->talkClass, "gdcSetFont", "(DDDILjava/lang/String;)V");
        if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, gc->cex, gc->ps, gc->lineheight, gc->fontface, s);
        else gdWarning("checkGC.gdcSetFont: can't get mid");
    }
    memcpy(&lastGC, gc, sizeof(lastGC));
}

/* re-set the GC - i.e. send commands for all monitored GC entries */
static void sendAllGC(JNIEnv *env, newXGDDesc *xd, R_GE_gcontext *gc) {
    printf("Basic GC:\n col=%08x\n fill=%08x\n gamma=%f\n lwd=%f\n lty=%08x\n cex=%f\n ps=%f\n lineheight=%f\n fontface=%d\n fantfamily=\"%s\"\n\n",
	 gc->col, gc->fill, gc->gamma, gc->lwd, gc->lty,
	 gc->cex, gc->ps, gc->lineheight, gc->fontface, gc->fontfamily);
    sendGC(env, xd, gc, 1);
}

static void newXGD_Activate(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdActivate", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
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
}

static void newXGD_Clip(double x0, double x1, double y0, double y1,  NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
       
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdClip", "(DDDD)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, x0, x1, y0, y1);
}

static void newXGD_Close(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdClose", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
}

static void newXGD_Deactivate(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdDeactivate", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
}

static void newXGD_Hold(NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdHold", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);
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
            return TRUE;
        }        
    }
    
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
}

static void newXGD_Mode(int mode, NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdMode", "(I)V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid, mode);
}

static void newXGD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd)
{
    newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
    JNIEnv *env = getJNIEnv();
    jmethodID mid;
    
    if(!env || !xd || !xd->talk) return;
    
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdNewPage", "()V");
    if (mid) (*env)->CallVoidMethod(env, xd->talk, mid);

    /* this is an exception - we send all GC attributes just after the NewPage command */
    sendAllGC(env, xd, gc);
}

Rboolean newXGD_Open(NewDevDesc *dd, newXGDDesc *xd,  char *dsp, double w, double h, char *host, int port)
{
   if (initJavaGD(xd)) return FALSE;

   xd->fill = 0xffffffff; /* transparent, was R_RGB(255, 255, 255); */
   xd->col = R_RGB(0, 0, 0);
   xd->canvas = R_RGB(255, 255, 255);
   xd->windowWidth = w;
   xd->windowHeight = h;

   {
       newXGDDesc *xd = (newXGDDesc *) dd->deviceSpecific;
       JNIEnv *env = getJNIEnv();
       jmethodID mid;
   
       if(!env || !xd || !xd->talk) return;
   
       /* we're not using dsp atm! */
       mid = (*env)->GetMethodID(env, xd->talkClass, "gdOpen", "(DD)V");
       if (mid)
           (*env)->CallVoidMethod(env, xd->talk, mid, w, h);
       else
           return FALSE;
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
            return 0;
        }
        memcpy(dae,ct,sizeof(double)*n);
        (*env)->ReleaseDoubleArrayElements(env, da, dae, 0);
    }
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
    mid = (*env)->GetMethodID(env, xd->talkClass, "gdStrText", "(DDLjava/lang/String;)D");
    if (mid)
        (*env)->CallVoidMethod(env, xd->talk, mid, x, y, s);
    (*env)->DeleteLocalRef(env, s);  
}

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

void jgdCheckExceptions(JNIEnv *env) {
    jthrowable t=(*env)->ExceptionOccurred(env);
    if (t) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
}

#define PATH_SEPARATOR ':'
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
        printf("Can't create Java Virtual Machine\n");
        return -1;
    }
    return 0;
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
        jclass c=(*env)->FindClass(env, "org/rosuda/javaGD/JavaGD");
        if (!c) c=(*env)->FindClass(env, "org/rosuda/javaGD/JavaGD");
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
    
    printf("Successfully instantiated JavaGD\n");
    return 0;
}
