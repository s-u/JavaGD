JavaGD <- function(name="JavaGD", w=400, h=300, ps=12) {
  invisible(.C("newJavaGD",as.character(name),as.numeric(w),as.numeric(h),as.numeric(ps)))
}

.getJavaGDObject <- function(devNr) {
    a<-.C("javaGDobject",as.integer(devNr-1),as.integer(0))
    optr<-a[[2]]
    if (optr==0) return (NULL)
    if (exists(".jmkref")) .jmkref(optr) else {
        o<-list(jobj=optr,jclass="java/lang/Object")
        class(o)<-"jobjRef"
        o
    }
}
