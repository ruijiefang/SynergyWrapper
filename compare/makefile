ifndef SIZE
  SIZE = 500
endif

all: mtx
mtx : matrix.c
	mpicc -O3 -DSZ=$(SIZE) -o mm matrix.c 
clean:
	rm -f mm
