BASE=../../
SHOAL=$(BASE)/shoal/

LIBS=-L$(BASE)/contrib/numactl-2.0.9 -lnuma\
	 -L$(BASE)/contrib/papi-5.3.0/src -lpapi\
	 -L$(BASE)/contrib/papi-5.3.0/src/libpfm4/lib -lpfm\
	 -L$(SHOAL) -lshl

OPTS=-Wall -g -I$(SHOAL)/inc
TARGET=simple

$(TARGET): main.c
	$(MAKE) -C $(SHOAL)
	$(CXX) $(OPTS) $< $(LIBS) -o $@


clean:
	$(MAKE) -C $(SHOAL) clean
	rm -f $(TARGET)
