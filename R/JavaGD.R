JavaGD <- function(name="JavaGD", w=400, h=300, ps=12) {
  invisible(.C("newJavaGD",as.character(name),as.numeric(w),as.numeric(h),as.numeric(ps),PACKAGE="JavaGD"))
}

.getJavaGDObject <- function(devNr) {
    a<-.Call("javaGDobjectCall",as.integer(devNr-1),PACKAGE="JavaGD")
    if (!is.null(a)) {
    	if (exists(".jmkref")) a <- .jmkref(a)
	else stop(".jmkref is not available. Please use rJava 0.3 or higher.")
    }
}
