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
