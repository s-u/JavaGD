.First.lib <- function(libname, pkgname) {
  # For MacOS X we have to remove /usr/X11R6/lib from the DYLD_LIBRARY_PATH
  # because it would override Apple's OpenGL framework (which is loaded
  # by JavaVM implicitly)
  Sys.putenv("DYLD_LIBRARY_PATH"=gsub("/usr/X11R6/lib","",Sys.getenv("DYLD_LIBRARY_PATH")))
  library.dynam("JavaGD", pkgname, libname)
}

JavaGD <- function(name="JavaGD", w=400, h=300) {
  invisible(.C("newXGD",as.character(name),as.numeric(w),as.numeric(h)))
}
