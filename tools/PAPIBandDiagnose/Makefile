PAPI = /home/haoxu/tools/papi-5.4.1
MONITOR = /home/haoxu/tools/libmonitor/install

PAPIINC = $(PAPI)/install/include/ 
MONITORINC = $(MONITOR)/include/

PAPILIB = -L$(PAPI)/src -lpapi  

MONITORLIB = -L$(MONITOR)/lib -lmonitor -lbfd


CC = gcc
CFLAGS += -I$(PAPIINC) -I$(MONITORINC)

#CFLAGS +=  -I$(NUMAINC) -I$(MONITORINC) -I$(PFMINC)


OS = $(shell uname)
CFLAGS += -Wall -fPIC -DMONITOR_DYNAMIC -DPEBS

TARGETS_CYC = PAPI_sample.o backtrace-symbol.o
TARGETS_CYC_SO = libPapiSample.so


ifeq ($(OS), SunOS)
   LDFLAGS =  $(PAPILIB) $(MONITORLIB) -lpthread -lcpc
   TARGETS = $(TARGETS_NTHD) $(TARGETS_SO)
else 
 ifeq ($(OS), AIX)
   CC = xlc
   LDFLAGS =  $(PAPILIB) $(MONITORLIB) -lpthread -lpmapi
   TARGETS = $(TARGETS_NTHD) $(TARGETS_SO) 
 else
    ifeq ($(OS), OSF1)
        LDFLAGS =  $(PAPILIB) $(MONITORLIB) -lpthread -lrt
        TARGETS = $(TARGETS_NTHD) $(TARGETS_SO) 
    else
        TARGETS = $(TARGETS_CYC) $(TARGETS_CYC_SO)
        LDFLAGS = $(PAPILIB) $(MONITORLIB) -lm -lpthread -lrt -lnuma
    endif
 endif
endif

all: $(TARGETS)

$(TARGETS_CYC):  PAPI_sample.c backtrace-symbol.c
	$(CC) -g  -c $(CFLAGS) PAPI_sample.c backtrace-symbol.c 
$(TARGETS_CYC_SO):  $(TARGETS_CYC)
	$(CC) -shared -g -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o $(TARGETS)

