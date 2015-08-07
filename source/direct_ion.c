#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "atomic.h"
#include "python.h"
#include "recipes.h"

#include <gsl/gsl_sf_expint.h>	//We need this gsl library to evaluate the first exponential integral

/* direct_ion contains the routines relating to direct (collisional) ionization and 
threebody recombination */

/************************************************************
                                    University of Southampton
Synopsis:
  compute_di_coeffs computes the rates due to direct (collisional)
  ionization from Dere data.

Arguments:

       double t_e       electron temperature        
      
Returns:
  0 on success
       
Description:

Notes: 

History:
  
************************************************************/

int
compute_di_coeffs (T)
     double T;
{
  int n;

  for (n = 0; n < nions; n++)
    {
      if (ion[n].dere_di_flag == 0)
	{
    //printf ("NO COEFFS\n");
	  di_coeffs[n] = 0.0;
	}

      else
	{
	  di_coeffs[n] = q_ioniz_dere(n, T);
	}

    }		//End of loop over ions

  return (0);
}

/************************************************************
                                    University of Southampton
Synopsis:
  compute_qrecomb_coeffs computes the rates for three body recombination
  from Dere data.

Arguments:

       double t_e       electron temperature        
      
Returns:
  0 on success
       

Description:


Notes: 


History:
  1508 JM Coded

************************************************************/

int
compute_qrecomb_coeffs(T)
     double T;
{
  int n, nvmin, ntmin;
  struct topbase_phot *xtop;

  for (n = 0; n < nions; n++)
    {
      if (ion[n].dere_di_flag == 0)
  {
    //printf ("NO COEFFS\n");
    qrecomb_coeffs[n] = 0.0;
  }

      else
  {
    /* we need to know about the bound-free jump, so we need the details
       from the ground state cross-section for this ion */

    ntmin = ion[n].ntop_ground;  /* We only ever use the ground state cont_ptr. 
                                       This is for topbase */
    nvmin = ion[n].nxphot;

    if (ion[n].phot_info > 0)  //topbase or hybrid VFKY (GS)+TB excited
      { 
        xtop = &phot_top[ntmin];
      }
    else if (ion[n].phot_info == 0)  // verner
      {   //just the ground state ionization fraction.
        xtop = &phot_top[nvmin];
      }

    qrecomb_coeffs[n] = q_recomb_dere(xtop, T);
  }

    }   //End of loop over ions

  return (0);
}



/************************************************************
                                    University of Southampton
Synopsis:
  total_di computed the total cooling due to direct (collisional)
  ionization from Dere data.

Arguments:

       WindPtr one      pointer to cell
       double t_e       electron temperature        
      
Returns:
  The total di cooling
       
Description:

Notes: 

History:
************************************************************/

double
total_di (one, t_e)
     WindPtr one;		// Pointer to the current wind cell - we need the cell volume, this is not in the plasma structure
     double t_e;		//Current electron temperature of the cell

{
  double x;			       //The returned variable
  int nplasma;			   //The cell number in the plasma array
  PlasmaPtr xplasma;   //pointer to the relevant cell in the plasma structure
  int n;			         //loop pointers


  nplasma = one->nplasma;	        //Get the correct plasma cell related to this wind cell
  xplasma = &plasmamain[nplasma];	//copy the plasma structure for that cell to local variable
  x = 0;			                    //zero the luminosity


  compute_di_coeffs (t_e);	      //Calculate the DR coefficients for this cell


  for (n = 0; n < nions; n++)
    {
      //We have no DR for this ion, or it's a macro-atom
      if (ion[n].dere_di_flag == 0 || )	
	{
	  x += 0.0;		//Add nothing to the sum of coefficients
	}
      else
	{
    
	  x += xplasma->vol * xplasma->ne * xplasma->density[n] * di_coeffs[n] *
	    dere_di_rate[ion[n].nxderedi].xi * EV2ERGS;

    //printf ("n=%i V=%e ne=%e rho=%e coeff=%e xi=%e cooling=%e\n",n, V , 
    //xplasma->ne , xplasma->density[n] , di_coeffs[n] ,
    //dere_di_rate[ion[n].nxderedi].xi*EV2ERGS,x);
	}
    }
  return (x);
}


/************************************************************
                                    University of Southampton
Synopsis:
  total_qrecomb computed the total cooling due to thrree body (collisional)
  ionization from Dere data.

Arguments:

       WindPtr one      pointer to cell
       double t_e       electron temperature        
      
Returns:
  The total di cooling
       
Description:

Notes: 
  This routine is very similar to total_di.

History:
************************************************************/

double
total_qrecomb (one, t_e)
     WindPtr one;   // Pointer to the current wind cell - we need the cell volume, this is not in the plasma structure
     double t_e;    //Current electron temperature of the cell

{
  double x;            //The returned variable
  int nplasma;         //The cell number in the plasma array
  PlasmaPtr xplasma;   //pointer to the relevant cell in the plasma structure
  int n;               //loop pointers


  nplasma = one->nplasma;         //Get the correct plasma cell related to this wind cell
  xplasma = &plasmamain[nplasma]; //copy the plasma structure for that cell to local variable
  x = 0;                          //zero the luminosity


  compute_qrecomb_coeffs (t_e);        //Calculate the DR coefficients for this cell


  for (n = 0; n < nions; n++)
    {
      if (ion[n].dere_di_flag == 0) //We have no DR for this ion.
  {
    x += 0.0;   //Add nothing to the sum of coefficients
  }
      else
  {

    x += xplasma->vol * xplasma->ne * xplasma->ne * xplasma->density[n] * qrecomb_coeffs[n] *
      dere_di_rate[ion[n].nxderedi].xi * EV2ERGS;

    //printf ("n=%i V=%e ne=%e rho=%e coeff=%e xi=%e cooling=%e\n",n, V , 
    //xplasma->ne , xplasma->density[n] , di_coeffs[n] ,
    //dere_di_rate[ion[n].nxderedi].xi*EV2ERGS,x);
  }
    }
  return (x);
}






/******************************************************************************/

/* q_ioniz_dere. This returns the collisional ionization co-efficient from Dere
data.
*/
/*could store global variables to avoid repeated interpolations */
//double t_e_last;
//double nion_last;

double
q_ioniz_dere (nion, t_e)
     int nion;
     double t_e;
{
  double coeff, t, scaled_t;
  double exp_int, dt, drdt, rate;
  int nrec, imax, imin, i;


  nrec = ion[nion].nxderedi;

  /* find the correct coefficient */


  t = (BOLTZMANN * t_e) / (dere_di_rate[nrec].xi * EV2ERGS);
  scaled_t = 1.0 - ((log (2.0)) / (log (2.0 + t)));

  if (scaled_t < dere_di_rate[nrec].temps[0])  //we are below the range of DI data data
    {

      Log_silent("compute_di_coeffs: Requested temp %e is below limit of data for ion %i(Tmin= %e)\n",
                  scaled_t, nion, dere_di_rate[nrec].temps[0]);
      //rate = rates[0];
      imax = 1;
      imin = 0;
    }

  else if (scaled_t >= dere_di_rate[nrec].temps[dere_di_rate[nrec].nspline - 1]) //we are above the range of GS data
    {
      Log_silent("compute_di_coeffs: Requested temp %e is above limit (%e) of data for ion %i\n",
                  scaled_t, dere_di_rate[nrec].temps[dere_di_rate[nrec].nspline - 1], nion);

      //rate = rates[BAD_GS_RR_PARAMS - 1];
      imax = dere_di_rate[nrec].nspline - 1;
      imin = dere_di_rate[nrec].nspline - 2;
      //We will try to extrapolate.
    }

  else      //We must be within the range of tabulated data
      {
        for (i = 0; i < dere_di_rate[nrec].nspline - 1; i++)
    {
      if (dere_di_rate[nrec].temps[i] <= scaled_t && 
          scaled_t < dere_di_rate[nrec].temps[i + 1])  //We have bracketed the correct temperature
        {
          imin = i;
          imax = i + 1;
        }
    }
      /* NSH 140313 - changed the following lines to interpolate in log space */
      }

  //if (ion[n].z==6 && ion[n].istate==4){
  //
  //printf ("T=%e Interploating between %e(%e) and %e(%e)\n",scaled_t,rates[imin],temps[imin],rates[imax],temps[imax]); 
  //}

  drdt = ((dere_di_rate[nrec].rates[imax]) - (dere_di_rate[nrec].rates[imin])) / ((dere_di_rate[nrec].temps[imax]) - (dere_di_rate[nrec].temps[imin]));
  dt = ((scaled_t) - (dere_di_rate[nrec].temps[imin]));
  rate = dere_di_rate[nrec].rates[imin] + drdt * dt;

  coeff = pow (t, -0.5) * pow (dere_di_rate[nrec].xi, -1.5) * rate;

  if (exp (-1.0 / t) < (1.0 / VERY_BIG))
    {
      exp_int = 0.0;
    }
  else
    {
      exp_int = gsl_sf_expint_E1 (1.0 / t); //Evaluate the first exponential integral using gsl library.
    }

  coeff *= exp_int;
  
  return (coeff);
}


/******************************************************************************/

/* q_ioniz. This returns the collisional ionization co-efficient
Calculated following equation 5-79 of Mihalas.
*/

double
q_ioniz (cont_ptr, electron_temperature)
     struct topbase_phot *cont_ptr;
     double electron_temperature;
{
  double coeff;
  double gaunt;
  double u0;
  int nion;

  /* these next two quantities only used in Hydrogen, no Dere data case */
  gaunt = 0.1;      //for now - from Mihalas for hydrogen
  u0 = cont_ptr->freq[0] * H_OVER_K / electron_temperature;
  nion = cont_ptr->nion;


  /* if ion[n].dere_di_flag == 1 then we have direct ionization data for this ion
     only do this if it is the ground state */
  if (ion[nion].dere_di_flag == 1 && config[cont_ptr->nlev].ilv == 1)
    {
      coeff = q_ioniz_dere(nion, electron_temperature);
    }

  /* if we are Hydrogen then use the approximation from Mihalas */
  else if (ion[nion].z == 1)
    {
      coeff = 1.55e13 / sqrt (electron_temperature) * gaunt * cont_ptr->x[0] *
        exp (-1. * u0) / u0;
    }

  /* otherwise return 0 */
  else
    coeff = 0.0;


  return (coeff);
}

/******************************************************************************/

/* q_recomb_dere. This returns the collisional recombination co-efficient
Calculated from inverse of q_ioniz_dere.

This equation comes from considering TE and getting the expression
q_recomb = 2.07e-16 * gl/gu * exp(E/kT) * q_ioniz
then substituting the above expression for q_ioniz.
*/

double
q_recomb_dere (cont_ptr, electron_temperature)
     struct topbase_phot *cont_ptr;
     double electron_temperature;
{
  int nion;
  double u0;
  double gaunt, coeff;

  gaunt = 0.1;      //for now - from Mihalas for hydrogen
  u0 = cont_ptr->freq[0] * H_OVER_K / electron_temperature;
  nion = cont_ptr->nion;
  /* if ion[n].dere_di_flag == 1 then we have direct ionization data for this ion
     only do this if it is the ground state */
  if (ion[nion].dere_di_flag == 1 && config[cont_ptr->nlev].ilv == 1)
    {
      coeff = 2.07e-16 * config[cont_ptr->nlev].g / config[cont_ptr->uplev].g;
      coeff *= exp(u0);
      coeff *= q_ioniz_dere(nion, electron_temperature);
    }
  else 
    coeff = 0.0;

  return (coeff);
}


/******************************************************************************/

/* q_recomb. This returns the collisional recombination co-efficient
Calculated from inverse of q_ioniz.

JM 1301 -- Edited this to avoid need to call q_ioniz and exponential.
Should improve speed and stability

This equation comes from considering TE and getting the expression
q_recomb = 2.07e-16 * gl/gu * exp(E/kT) * q_ioniz
then substituting the above expression for q_ioniz.
*/

double
q_recomb (cont_ptr, electron_temperature)
     struct topbase_phot *cont_ptr;
     double electron_temperature;
{
  double coeff;
  double gaunt, u0;
  int nion;

  nion = cont_ptr->nion;
  gaunt = 0.1;      //for now - from Mihalas for hydrogen
  u0 = cont_ptr->freq[0] * H_OVER_K / electron_temperature;

  /* if ion[n].dere_di_flag == 1 then we have direct ionization data for this ion
     only do this if it is the ground state */
  if (ion[nion].dere_di_flag == 1 && config[cont_ptr->nlev].ilv == 1)
    {
      coeff = q_recomb_dere(cont_ptr, electron_temperature);
    }

  /* if we are Hydrogen then use the approximation from Mihalas */
  else if (ion[nion].z == 1)
    {
      coeff = 3.2085e-3  / electron_temperature * gaunt * cont_ptr->x[0]; // normal constants * 1/T times gaunt * cross section

      coeff /= cont_ptr->freq[0] * H_OVER_K;      // divide by h nu / k
      coeff *= config[cont_ptr->nlev].g / config[cont_ptr->uplev].g;
    }

  /* otherwise return 0 */
  else
    coeff = 0.0;


  return (coeff);
}
