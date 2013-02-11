# This is the makefile for the python related programs
#
# History
# 05jan	ksl	54f  Modified Makefile so that the Version number is automatically
# 		copied to version.h  Also fixed so that one does not need to put
# 		a new filename in two separate places, and so that the latest
# 		compile version automatically becomve shte 
# 05apr	ksl	55c   Modified in a way that implies that one must have gsl as part
# 		of the Python directory tree.  This is because I was having problems
# 		with gsl after we went to Redhat Enterprise at the Institute, and
# 		so that Stuart and I could standardise on the distribution.
# 08jul	ksl	Removed pfop from routines so no need to complile with g77
CC = gcc
FC = g77
# FC = gfortran

INCLUDE = ../../include
INCLUDE2 = ../../gsl/include

LIB = ../../lib
LIB2 = ../../gsl/lib
BIN = ../../bin

# use pg when you want to use gprof the profiler
FFLAGS = -g -pg   
CFLAGS = -g -pg -Wall -I$(INCLUDE) -I$(INCLUDE2)  
 
# Use this for large runs
#  CFLAGS = -O3 -Wall -I$(INCLUDE)  -I$(INCLUDE2)
#  FFLAGS =     

# next LIne for debugging when concerned about memory problems
# LDFLAGS= -L$(LIB) -L$(LIB2)  -lm -lkpar -lcfitsio -lgsl -lgslcblas ../../duma_2_5_3/libduma.a -lpthread
LDFLAGS= -L$(LIB) -L$(LIB2)  -lm -lkpar -lcfitsio -lgsl -lgslcblas 

#Note that version should be a single string without spaces. 
VERSION = 68c
CHOICE=1             // Compress plasma as much as possible
# CHOICE=0           //  Keep relation between plasma and wind identical

startup:
	echo "#define VERSION " \"$(VERSION)\" > version.h
	echo "#define CHOICE"   $(CHOICE) >> version.h


foo: foo.o signal.o time.o
	gcc ${cfllags} foo.o signal.o time.o ${LDFLAGS}  -o foo

python_objects = bb.o get_atomicdata.o python.o photon2d.o photon_gen.o \
		saha.o spectra.o wind2d.o wind.o  vector.o debug.o recipes.o \
		trans_phot.o phot_util.o resonate.o radiation.o \
		wind_updates2d.o windsave.o extract.o pdf.o roche.o random.o \
		stellar_wind.o proga.o corona.o knigge.o thierry.o disk.o\
		lines.o  continuum.o get_models.o emission.o recomb.o diag.o \
		sv.o ionization.o  ispy.o   levels.o gradv.o reposition.o \
		anisowind.o util.o density.o  detail.o bands.o time.o \
		matom.o estimators.o wind_sum.o yso.o elvis.o cylindrical.o rtheta.o spherical.o  \
		cylind_var.o bilinear.o gridwind.o partition.o signal.o auger_ionization.o


python_source= bb.c get_atomicdata.c python.c photon2d.c photon_gen.c \
		saha.c spectra.c wind2d.c wind.c  vector.c debug.c recipes.c \
		trans_phot.c phot_util.c resonate.c radiation.c \
		wind_updates2d.c windsave.c extract.c pdf.c roche.c random.c \
		stellar_wind.c proga.c corona.c knigge.c thierry.c disk.c\
		lines.c  continuum.c emission.c recomb.c diag.c \
		sv.c ionization.c  ispy.c  levels.c gradv.c reposition.c \
		anisowind.c util.c density.c  detail.c bands.c time.c \
		matom.c estimators.c wind_sum.c yso.c elvis.c cylindrical.c rtheta.c spherical.c  \
		cylind_var.c bilinear.c gridwind.c partition.c signal.c auger_ionization.c

additional_py_wind_source = py_wind_sub.c py_wind_ion.c py_wind_write.c py_wind_macro.c

prototypes: 
	cp templates.h templates.h.old
	cproto -I$(INCLUDE)  -I$(INCLUDE2) $(python_source) ${additional_py_wind_source} > foo.h      
	cp foo.h templates.h

python: startup  $(python_objects)
	gcc  ${CFLAGS} $(python_objects) $(LDFLAGS) -o python
		cp $@ $(BIN)/py
		mv $@ $(BIN)/py$(VERSION)


py_wind_objects = py_wind.o get_atomicdata.o py_wind_sub.o windsave.o py_wind_ion.o \
		emission.o recomb.o util.o detail.o \
		pdf.o random.o recipes.o saha.o \
		stellar_wind.o sv.o proga.o corona.o knigge.o thierry.o disk.o\
		lines.o vector.o wind2d.o wind.o  ionization.o  py_wind_write.o levels.o \
		radiation.o gradv.o phot_util.o anisowind.o resonate.o density.o \
		matom.o estimators.o yso.o elvis.o photon2d.o cylindrical.o rtheta.o spherical.o  \
		cylind_var.o bilinear.o gridwind.o py_wind_macro.o partition.o auger_ionization.o



py_wind: startup $(py_wind_objects)
	gcc $(CFLAGS) $(py_wind_objects) $(LDFLAGS) -o py_wind
	cp $@ $(BIN)
	mv $@ $(BIN)/py_wind$(VERSION)

py_smooth: py_smooth.o 
	$(CC) $(CFLAGS) py_smooth.o  $(LDFLAGS)  -o py_smooth
		mv $@ $(BIN)

test_bb: bb.o test_bb.o pdf.o recipes.o bilinear.o time.o 
	gcc  ${CFLAGS} bb.o pdf.o test_bb.o  recipes.o bilinear.o time.o $(LDFLAGS) -o test_bb
	


t_bilinear:  t_bilinear.o bilinear.o  
	$(CC) $(CFLAGS) t_bilinear.o  bilinear.o  $(LDFLAGS)  -o t_bilinear

	
plot_roche: plot_roche.o roche.o vector.o phot_util.o recipes.o 
	${CC} ${CFLAGS} plot_roche.o roche.o vector.o phot_util.o recipes.o  \
		$(LDFLAGS) -o plot_roche
		mv $@ $(BIN)/plot_roche

# This diagnostic routine has not been kept up to date
py_ray: bb.o get_atomicdata.o py_ray.o photon2d.o photon_gen.o \
		saha.o spectra.o wind2d.o wind.o  vector.o debug.o recipes.o \
		trans_phot.o phot_util.o resonate.o radiation.o \
		wind_updates2d.o windsave.o extract.o pdf.o roche.o random.o \
		stellar_wind.o sv.o proga.o corona.o knigge.o thierry.o disk.o\
		lines.o continuum.o get_models.o emission.o recomb.o util.o anisowind.o \
		sv.o ionization.o  ispy.o   levels.o gradv.o reposition.o \
		density.o  detail.o  bands.o \
		yso.o elvis.o cylindrical.o rtheta.o \
		matom.o estimators.o wind_sum.o
	gcc  ${CFLAGS} py_ray.o get_atomicdata.o photon2d.o photon_gen.o \
		bb.o detail.o \
		saha.o spectra.o wind2d.o wind.o  vector.o debug.o recipes.o \
		trans_phot.o phot_util.o resonate.o radiation.o \
		stellar_wind.o   corona.o util.o anisowind.o \
		wind_updates2d.o windsave.o extract.o ispy.o bands.o\
		pdf.o roche.o random.o continuum.o get_models.o thierry.o \
		lines.o ionization.o emission.o  recomb.o reposition.o \
		sv.o proga.o knigge.o disk.o  levels.o gradv.o density.o \
		yso.o elvis.o cylindrical.o rtheta.o \
		matom.o estimators.o wind_sum.o \
		$(LDFLAGS) -o py_ray
		cp $@ $(BIN)/py_ray
		mv $@ $(BIN)/py_ray$(VERSION)




# This diagnostic routine has not been kept up to date
py_grid: bb.o get_atomicdata.o py_grid.o photon2d.o photon_gen.o \
		saha.o spectra.o wind2d.o wind.o  vector.o debug.o recipes.o \
		trans_phot.o phot_util.o resonate.o radiation.o \
		wind_updates2d.o windsave.o extract.o pdf.o roche.o random.o \
		stellar_wind.o sv.o proga.o corona.o knigge.o thierry.o disk.o\
		lines.o continuum.o get_models.o emission.o recomb.o util.o anisowind.o \
		sv.o ionization.o  ispy.o   levels.o gradv.o reposition.o \
		yso.o elvis.o cylindrical.o rtheta.o \
		matom.o estimators.o wind_sum.o \
		density.o bands.o detail.o 
	gcc  ${CFLAGS} py_grid.o get_atomicdata.o photon2d.o photon_gen.o \
		bb.o\
		saha.o spectra.o wind2d.o wind.o  vector.o debug.o recipes.o \
		trans_phot.o phot_util.o resonate.o radiation.o \
		stellar_wind.o   corona.o util.o anisowind.o \
		wind_updates2d.o windsave.o extract.o ispy.o bands.o\
		pdf.o roche.o random.o continuum.o get_models.o thierry.o \
		lines.o ionization.o emission.o  recomb.o reposition.o \
		sv.o proga.o knigge.o disk.o  levels.o gradv.o density.o \
		yso.o elvis.o cylindrical.o rtheta.o \
		matom.o estimators.o wind_sum.o \
		detail.o \
		$(LDFLAGS) -o py_grid
		cp $@ $(BIN)/py_grid
		mv $@ $(BIN)/py_grid$(VERSION)

# Balance does compile but has not been used much if at all in the recent past.  
balance_sources = balance_abso.c balance_bb.c balance.c balance_gen.c balance_sub.c bal_photon2d.c bal_trans_phot.c plane.c \
		  partition.c

startup_balance: startup $(balance_sources)
	cproto -I$(INCLUDE)  -I$(INCLUDE2) $(balance_sources) > balance_templates.h

balance: balance.o balance_sub.o balance_gen.o balance_abso.o \
		emission.o recomb.o balance_bb.o gradv.o \
		bb.o pdf.o sv.o vector.o wind_updates2d.o windsave.o \
		radiation.o continuum.o get_models.o bal_trans_phot.o phot_util.o extract.o \
		ionization.o saha.o recipes.o plane.o resonate.o ispy.o \
		roche.o stellar_wind.o  proga.o corona.o disk.o  knigge.o thierry.o \
		lines.o get_atomicdata.o random.o wind2d.o wind.o   bal_photon2d.o  levels.o  \
		util.o anisowind.o reposition.o density.o  detail.o bands.o matom.o estimators.o  bilinear.o   \
		spherical.o cylindrical.o cylind_var.o rtheta.o yso.o elvis.o gridwind.o wind_sum.o \
		partition.o 
	gcc   ${CFLAGS} balance.o balance_sub.o balance_gen.o   balance_abso.o \
		emission.o recomb.o balance_bb.o gradv.o  detail.o \
		get_atomicdata.o random.c wind2d.o wind.o  bal_trans_phot.o \
		bb.o pdf.o sv.o vector.o wind_updates2d.o windsave.o \
		radiation.o continuum.o get_models.o plane.o phot_util.o resonate.o levels.o \
		lines.o ionization.o saha.o recipes.o bal_photon2d.o thierry.o \
		extract.o ispy.o roche.o stellar_wind.o proga.o corona.o disk.o  knigge.o  \
		util.o anisowind.o reposition.o density.o bands.o matom.o estimators.o bilinear.o \
		spherical.o cylindrical.o cylind_var.o rtheta.o  yso.o elvis.o   gridwind.o wind_sum.o\
		partition.o \
		$(LDFLAGS) -o balance
	cp $@ $(BIN)/balance
	mv $@ $(BIN)/balance$(VERSION)


FILE = get_atomicdata.o atomic.o

libatomic.a:  get_atomicdata.o atomic.o
	ar ru libatomic.a get_atomicdata.o atomic.o
	ranlib libatomic.a
	mv libatomic.a $(LIB)
	cp atomic.h  $(INCLUDE)


clean :
	rm -f *.o  *~ 