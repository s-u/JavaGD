\name{JavaGD}
\alias{JavaGD}
\alias{javaGD}
\title{
  Create a new Java graphics device
}
\description{
  \code{JavaGD} initializes a new Java grapgics device and creates a new window.
}
\usage{
JavaGD(name="JavaGD", w=400, h=300)
}
\arguments{
  \item{name}{Name of the device}
  \item{w}{Initial width of the window (usually in pixels, but the
    actual interpretation depends on the Java implementation)}
  \item{h}{Initial height of the window}
}
\value{
  Currently the return value is implementation dependent, mostly just
  the return code as returned by the low-level C function
}
\seealso{
}
\examples{
\dontrun{
JavaGD()
}
}
\keyword{device}
