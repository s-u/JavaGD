.javaGD.get.size <- function(devNr=dev.cur()) {
    par<-rep(0,6)
    l<-.C("javaGDgetSize",as.integer(devNr-1),as.double(par))
    par<-l[[2]]
    if (par[6]==0) list() else list(x=par[1],y=par[2],width=(par[3]-par[1]),height=(par[4]-par[2]),dpiX=par[5],dpiY=par[6])
}

.javaGD.copy.device <- function(devNr=dev.cur(), device=pdf, width.diff=0, height.diff=0, ...) {
    s<-.javaGD.get.size(devNr)
    pd<-dev.cur()
    dev.set(devNr)
    dev.copy(device, width=s$width/s$dpiX+width.diff, height=s$height/s$dpiY+height.diff, ...)
    dev.off()
    dev.set(pd)
    invisible(devNr)
}

.javaGD.version <- function() {
	v<-.C("javaGDversion",as.integer(rep(0,4)))[[1]]
	list(major=v[1]%/%65536, minor=(v[1]%/%256)%%256, patch=(v[1]%%256), numeric=v[1])
}

.javaGD.set.display.parameters <- function(dpiX=100, dpiY=100, aspect=1) {
	invisible(.C("javaGDsetDisplayParam",as.double(c(dpiX, dpiY, aspect))))
}
