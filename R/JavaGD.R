JavaGD <- function(name="JavaGD", w=400, h=300, ps=12) {
  invisible(.C("newJavaGD",as.character(name),as.numeric(w),as.numeric(h),as.numeric(ps)))
}
