# Go programs to build
GO_PROGS = hist rep

all:

# Default target
#all: $(GO_PROGS)

# Build Go version of hist
hist: hist.go
	go build -o hist hist.go

# Build Go version of rep
rep: rep.go
	go build -o rep rep.go

# Clean build artifacts
clean:
	rm -f $(GO_PROGS)

# Install (reports are typically run from this directory, not installed system-wide)
install:
	@echo "Reports are run from this directory, no system installation needed"

.PHONY: all clean install
