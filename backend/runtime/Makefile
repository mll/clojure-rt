
CC = arch -arm64 clang
CCP = arch -arm64 clang++
AR = arch -arm64 ar
ARFLAGS = rcs
MYFLAGSOPT = -Ofast
MYFLAGS = -g 
CPPFLAGS = -std=c++17  
CFLAGS =  $(MYFLAGS) -I/opt/homebrew/include
PROFILE = -Ofast 

LIBS = -L/opt/homebrew/lib -lprotobuf

OBJFILES =  Object.o Integer.o PersistentList.o PersistentVector.o PersistentVectorNode.o String.o Double.o Nil.o Boolean.o sds.o pool.o 

default: all

all: 
	$(MAKE) runtime
runtime-tests: $(OBJFILES) runtime-tests.o
	$(CCP) -o runtime-tests $(PROFILE) $(LIBS) runtime-tests.o  $(OBJFILES) 
runtime: $(OBJFILES)
	$(AR) $(ARFLAGS) runtime.a  $(OBJFILES) 
clean:
	rm -f *.o
	rm runtime.a
Object.o: Object.c Object.h  
	$(CC) -c $(CFLAGS) Object.c
runtime-tests.o: runtime-tests.cpp
	$(CCP) -c $(CFLAGS) $(CPPFLAGS) runtime-tests.cpp
Integer.o: Integer.c Integer.h
	$(CC) -c $(CFLAGS) Integer.c
PersistentList.o: PersistentList.c PersistentList.h
	$(CC) -c $(CFLAGS) PersistentList.c
PersistentVector.o: PersistentVector.c PersistentVector.h
	$(CC) -c $(CFLAGS) PersistentVector.c
PersistentVectorNode.o: PersistentVectorNode.c PersistentVectorNode.h
	$(CC) -c $(CFLAGS) PersistentVectorNode.c
String.o: String.c String.h  
	$(CC) -c $(CFLAGS) String.c
Double.o: Double.c Double.h  
	$(CC) -c $(CFLAGS) Double.c
Nil.o: Nil.c Nil.h
	$(CC) -c $(CFLAGS) Nil.c
Boolean.o: Boolean.c Boolean.h  
	$(CC) -c $(CFLAGS) Boolean.c
sds.o: sds/sds.c sds/sds.h
	$(CC) -c $(CFLAGS) sds/sds.c
pool.o: pool/pool.c pool/pool.h
	$(CC) -c $(CFLAGS) pool/pool.c
