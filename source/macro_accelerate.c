#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "atomic.h"
#include "python.h"
//gsl matrix solvers
#include <gsl/gsl_block.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_mode.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>


/**********************************************************/
/** 
 * @brief calculate the matrix of probabilities for the accelerated macro-atom scheme 
 *
 * @param [in] PlasmaPtr  xplasma  
 * @param [in,out] double **matom_matrix 
 *        the 2D matrix array we will populate with normalised probabilities 
 *
 * @details
 * given an activation state, this routine calculates the probability that a packet
 * will deactivate from a given state. Let's suppose that the \f$(i,j)\f$-th element of matrix 
 * \f$Q\f$ contains the **jumping** probability from state i to state j, and the diagonals 
 * \f$(i,i)\f$ of matrix \f$R\f$ contain the emission probabilities from each state, then it 
 * can be shown that (see short notes from Vogl, or 
 * <a href="Ergon et al. 2018">https://ui.adsabs.harvard.edu/abs/2018A%26A...620A.156E</a>)
 * the quantity we want is then \f$B = N R\f$, where \f$N = (I - Q)^{-1}\f$, where \f$I\f$
 * is the identity matrix. This routine does this calculation.
 *
 **********************************************************/

void
calc_matom_matrix (xplasma, matom_matrix)
     PlasmaPtr xplasma;
     double **matom_matrix;
{
  MacroPtr mplasma;
  double t_e, ne;
  int nbbd, nbbu, nbfd, nbfu;
  int uplvl, target_level;
  double Qcont;
  struct lines *line_ptr;
  struct topbase_phot *cont_ptr;
  double rad_rate, coll_rate;
  int n, i, nn, mm;
  double Qcont_kpkt, bb_cont, sp_rec_rate, bf_cont, lower_density, density_ratio;
  double kpacket_to_rpacket_rate, norm, Rcont;
  double *a_data;
  mplasma = &macromain[xplasma->nplasma];       //telling us where in the matom structure we are

  t_e = xplasma->t_e;           //electron temperature 
  ne = xplasma->ne;             //electron number density

  /* allocate arrays for matrices and normalsiations
     we want to include all macro-atom levels + 1 kpacket levels */
  int nrows = nlevels_macro + 1;
  double **R_matrix = (double **) calloc (sizeof (double *), nrows);
  double **Q_matrix = (double **) calloc (sizeof (double *), nrows);
  double *Q_norm = (double *) calloc (sizeof (double), nrows);

  for (i = 0; i < nrows; i++)
  {
    R_matrix[i] = (double *) calloc (sizeof (double), nrows);
    Q_matrix[i] = (double *) calloc (sizeof (double), nrows);
  }

  /* initialise everything to zero */
  norm = 0.0;
  for (uplvl = 0; uplvl < nrows; uplvl++)
  {
    Q_norm[uplvl] = 0.0;
    for (target_level = 0; target_level < nrows; target_level++)
    {
      Q_matrix[uplvl][target_level] = 0.0;
      R_matrix[uplvl][target_level] = 0.0;
      matom_matrix[uplvl][target_level] = 0.0;
    }
  }

  /* loop over all macro-atom levels and populate the rate matrix */
  for (uplvl = 0; uplvl < nlevels_macro; uplvl++)
  {
    nbbd = config[uplvl].n_bbd_jump;    //store these for easy access -- number of bb downward jumps
    nbbu = config[uplvl].n_bbu_jump;    // number of bb upward jump from this configuration
    nbfd = config[uplvl].n_bfd_jump;    // number of bf downward jumps from this transition
    nbfu = config[uplvl].n_bfu_jump;    // number of bf upward jumps from this transiion


    /* bound-bound */
    for (n = 0; n < nbbd; n++)
    {

      line_ptr = &line[config[uplvl].bbd_jump[n]];

      rad_rate = (a21 (line_ptr) * p_escape (line_ptr, xplasma));
      coll_rate = q21 (line_ptr, t_e);  // this is multiplied by ne below

      if (coll_rate < 0)
      {
        coll_rate = 0;
      }

      bb_cont = rad_rate + (coll_rate * ne);

      target_level = line_ptr->nconfigl;

      //internal jump to another macro atom level
      Q_matrix[uplvl][target_level] += Qcont = bb_cont * config[target_level].ex;       //energy of lower state

      //jump to the k-packet pool (we used to call this "deactivation")
      Q_matrix[uplvl][nlevels_macro] += Qcont_kpkt = (coll_rate * ne) * (config[uplvl].ex - config[target_level].ex);   //energy of lower state

      //deactivation back to r-packet
      R_matrix[uplvl][uplvl] += Rcont = rad_rate * (config[uplvl].ex - config[target_level].ex);        //energy difference

      Q_norm[uplvl] += Qcont + Qcont_kpkt + Rcont;
    }

    /* bound-free */
    for (n = 0; n < nbfd; n++)
    {

      cont_ptr = &phot_top[config[uplvl].bfd_jump[n]];  //pointer to continuum
      if (n < 25)               //?
      {
        sp_rec_rate = mplasma->recomb_sp[config[uplvl].bfd_indx_first + n];     //need this twice so store it
        bf_cont = (sp_rec_rate + q_recomb (cont_ptr, t_e) * ne) * ne;
      }
      else
      {
        bf_cont = sp_rec_rate = 0.0;
      }


      target_level = phot_top[config[uplvl].bfd_jump[n]].nlev;

      if (bf_cont > 0.0)
      {

        //internal jump to another macro atom level
        Q_matrix[uplvl][target_level] += Qcont = bf_cont * config[target_level].ex;     //energy of lower state

        //jump to the k-packet pool (we used to call this "deactivation")
        Q_matrix[uplvl][nlevels_macro] += Qcont_kpkt = q_recomb (cont_ptr, t_e) * ne * ne * (config[uplvl].ex - config[target_level].ex);       //energy difference

        //deactivation back to r-packet
        R_matrix[uplvl][uplvl] += Rcont = ne * sp_rec_rate * (config[uplvl].ex - config[target_level].ex);      //energy difference

        Q_norm[uplvl] += Qcont + Qcont_kpkt + Rcont;
      }
    }

    /* Now upwards jumps. */

    /* bound-bound */
    for (n = 0; n < nbbu; n++)
    {
      line_ptr = &line[config[uplvl].bbu_jump[n]];
      rad_rate = (b12 (line_ptr) * mplasma->jbar_old[config[uplvl].bbu_indx_first + n]);

      coll_rate = q12 (line_ptr, t_e);  // this is multiplied by ne below

      if (coll_rate < 0)
      {
        coll_rate = 0;
      }
      target_level = line[config[uplvl].bbu_jump[n]].nconfigu;
      Q_matrix[uplvl][target_level] += Qcont = ((rad_rate) + (coll_rate * ne)) * config[uplvl].ex;      //energy of lower state


      Q_norm[uplvl] += Qcont;
    }

    /* bound-free */
    for (n = 0; n < nbfu; n++)
    {
      /* For bf ionization the jump probability is just gamma * energy
         gamma is the photoionisation rate. Stimulated recombination also included. */
      cont_ptr = &phot_top[config[uplvl].bfu_jump[n]];  //pointer to continuum

      /* first let us take care of the situation where the lower level is zero or close to zero */
      lower_density = den_config (xplasma, cont_ptr->nlev);
      if (lower_density >= DENSITY_PHOT_MIN)
      {
        density_ratio = den_config (xplasma, cont_ptr->uplev) / lower_density;
      }
      else
        density_ratio = 0.0;

      target_level = phot_top[config[uplvl].bfu_jump[n]].uplev;
      Qcont = (mplasma->gamma_old[config[uplvl].bfu_indx_first + n] - (mplasma->alpha_st_old[config[uplvl].bfu_indx_first + n] * xplasma->ne * density_ratio) + (q_ioniz (cont_ptr, t_e) * ne)) * config[uplvl].ex;     //energy of lower state

      /* this error condition can happen in unconverged hot cells where T_R >> T_E.
         for the moment we set to 0 and hope spontaneous recombiantion takes care of things */
      /* note that we check and report this in check_stimulated_recomb() in estimators.c once a cycle */
      if (Qcont < 0.)           //test (can be deleted eventually SS)
      {
        //Error ("Negative probability (matom, 6). Abort?\n");
        Qcont = 0.0;

      }
      Q_matrix[uplvl][target_level] += Qcont;
      Q_norm[uplvl] += Qcont;
    }


  }

  /* Now need to do k-packet processes */

  int escape_dummy = 0;
  int istat_dummy = 0;
  fill_kpkt_rates (xplasma, &escape_dummy, &istat_dummy);
  /* Cooling due to collisional transitions in lines and collision ionization [for macro atoms] constitute internal transitions from the k-packet pool to macro atom states. */
  kpacket_to_rpacket_rate = 0.0;        // keep track of rate for kpacket_to_rpacket channel

  for (i = 0; i < nlines; i++)
  {
    if (line[i].macro_info == 1 && geo.macro_simple == 0)       //line is for a macro atom
    {
      target_level = line[i].nconfigu;
      Q_matrix[nlevels_macro][target_level] += Qcont = mplasma->cooling_bb[i];
      Q_norm[nlevels_macro] += Qcont;
    }
    else
    {
      /* the idea here is that if it is a simple line then it *must* create an r-packet eventually, so this is essentially
         a k->r transition */
      kpacket_to_rpacket_rate += mplasma->cooling_bb[i];
    }
  }

  for (i = 0; i < nphot_total; i++)
  {
    if (phot_top[i].macro_info == 1 && geo.macro_simple == 0)   //part of macro atom
    {
      target_level = phot_top[i].uplev;
      Q_matrix[nlevels_macro][target_level] += Qcont = mplasma->cooling_bf[i];
      Q_norm[nlevels_macro] += Qcont;

    }
    else
    {
      /* XXX - ask stuart about this! */
      kpacket_to_rpacket_rate += mplasma->cooling_bf_col[i];
    }
  }

  /* Cooling due to other processes will always contribute to k-packet -> r-packet channel */

  kpacket_to_rpacket_rate += mplasma->cooling_bftot;
  kpacket_to_rpacket_rate += mplasma->cooling_adiabatic;
  kpacket_to_rpacket_rate += mplasma->cooling_ff + mplasma->cooling_ff_lofreq;
  R_matrix[nlevels_macro][nlevels_macro] += Rcont = kpacket_to_rpacket_rate;
  Q_norm[nlevels_macro] += Rcont;

  /* end kpacket */

  /* now in one step, we multiply by the identity matrix and normalise the probabilities
     this means that what is now stored in Q_matrix is no longer Q, but N=(I - Q) using Vogl 
     notation. We check that Q_norm is 0, because some states (ground states) can have 0 
     jumping probabilities and so zero normalisation too */
  for (uplvl = 0; uplvl < nrows; uplvl++)
  {
    for (target_level = 0; target_level < nrows; target_level++)
    {
      if (Q_norm[uplvl] > 0.0)
      {
        Q_matrix[uplvl][target_level] = -1. * Q_matrix[uplvl][target_level] / Q_norm[uplvl];
      }
    }
    Q_matrix[uplvl][uplvl] = 1.0;

    if (Q_norm[uplvl] > 0.0)
    {
      R_matrix[uplvl][uplvl] = R_matrix[uplvl][uplvl] / Q_norm[uplvl];
    }
  }


  /* Check normalisation of the matrix. the diagonals of R, minus N which is now stored in 
     Q_matrix, should be zero */
  for (uplvl = 0; uplvl < nrows; uplvl++)
  {
    norm = R_matrix[uplvl][uplvl];
    for (target_level = 0; target_level < nlevels_macro + 1; target_level++)
    {
      norm -= Q_matrix[uplvl][target_level];
    }

    /* throw an error if this normalisation is not zero */
    /* note that the ground state is a special case here (improve error check) */
    if ((abs (norm) > 1e-15 && uplvl != ion[config[uplvl].nion].first_nlte_level) || sane_check (norm))
      Error ("calc_matom_matrix: matom accelerator matrix has bad normalisation for level %d: %8.4e\n", norm, uplvl);
  }

  /* This next line produces an array of the correct size to hold the rate matrix */
  a_data = (double *) calloc (sizeof (double), nrows * nrows);

  /* We now copy our rate matrix into the prepared matrix */
  for (mm = 0; mm < nrows; mm++)
  {
    for (nn = 0; nn < nrows; nn++)
    {
      a_data[mm * nrows + nn] = Q_matrix[mm][nn];
    }
  }

  /* now get ready for the matrix operations. first let's assign variables for use with GSL */
  gsl_matrix_view N;
  gsl_matrix *inverse_matrix;
  gsl_permutation *p, *pp;
  int ierr, s;

  /* create a view into the array we just created */
  N = gsl_matrix_view_array (a_data, nrows, nrows);

  /* permuations are special structures that contain integers 0 to nrows-1, which can
   * be manipulated */
  p = gsl_permutation_alloc (nrows);
  inverse_matrix = gsl_matrix_alloc (nrows, nrows);

  /* first we do the LU decomposition */
  ierr = gsl_linalg_LU_decomp (&N.matrix, p, &s);

  /* from the decomposition, get the inverse of the Q matrix */
  ierr = gsl_linalg_LU_invert (&N.matrix, p, inverse_matrix);

  /* We now copy our rate matrix into the prepared matrix */
  for (mm = 0; mm < nrows; mm++)
  {
    for (nn = 0; nn < nrows; nn++)
    {
      /* copy the matrix from the gsl object to the array we return to user */
      /* in Christian Vogl's notation this is doing his equation 3: B = (N * R)
         where N is the inverse matrix we have just calculated. */
      /* the reason this matrix multiplication is so simple here is because R is a diagonal matrix */
      matom_matrix[mm][nn] = gsl_matrix_get (inverse_matrix, mm, nn) * R_matrix[nn][nn];
    }
  }

  /* free memory */
  gsl_permutation_free (p);
  gsl_matrix_free (inverse_matrix);
  free (a_data);

  /* need to free each calloc-ed row of the matrixes */
  for (i = 0; i < nrows; i++)
  {
    free (R_matrix[i]);
    free (Q_matrix[i]);
  }
  free (R_matrix);
  free (Q_matrix);
  free (Q_norm);
}


/**********************************************************/
/** 
 * @brief calculate the cooling rates for the conversion of k-packets.
 *
 * @param [in] PlasmaPtr  xplasma  
 * @param [in,out] double **matom_matrix 
 *        the 2D matrix array we will populate with normalised probabilities 
 *
 *
 * @details
 * 
 *
 **********************************************************/

int
fill_kpkt_rates (xplasma, escape, p)
     PlasmaPtr xplasma;
     int *escape;
     PhotPtr p;
{

  int i;
  int ulvl;
  double cooling_bf[nphot_total];
  double cooling_bf_col[nphot_total];   //collisional cooling in bf transitions
  double cooling_bb[NLINES];
  double cooling_adiabatic;
  struct topbase_phot *cont_ptr;
  struct lines *line_ptr;
  double cooling_normalisation;
  double destruction_choice;
  double electron_temperature;
  double cooling_bbtot, cooling_bftot, cooling_bf_coltot;
  double lower_density, upper_density;
  double cooling_ff, upweight_factor;
  WindPtr one;

  MacroPtr mplasma;

  double coll_rate, rad_rate;
  double freqmin, freqmax;

  mplasma = &macromain[xplasma->nplasma];
  one = &wmain[xplasma->nwind];

  electron_temperature = xplasma->t_e;

  /* JM 1511 -- Fix for issue 187. We need band limits for free free packet
     generation (see call to one_ff below) */
  freqmin = xband.f1[0];
  freqmax = ALPHA_FF * xplasma->t_e / H_OVER_K;

  /* ksl This is a Bandaid for when the temperatures are very low */
  /* in this case cooling_ff should be low compared to cooling_ff_lofreq anyway */
  if (freqmax < 1.1 * freqmin)
  {
    freqmax = 1.1 * freqmin;
  }

  /* If the kpkt destruction rates for this cell are not known they are calculated here.  This happens
   * every time the wind is updated */

  if (mplasma->kpkt_rates_known != 1)
  {
    cooling_normalisation = 0.0;
    cooling_bftot = 0.0;
    cooling_bbtot = 0.0;
    cooling_ff = 0.0;
    cooling_bf_coltot = 0.0;

    /* Start of BF calculation */
    /* JM 1503 -- we used to loop over ntop_phot here, 
       but we should really loop over the tabulated Verner Xsections too
       see #86, #141 */
    for (i = 0; i < nphot_total; i++)
    {
      cont_ptr = &phot_top[i];
      ulvl = cont_ptr->uplev;

      if (cont_ptr->macro_info == TRUE && geo.macro_simple == FALSE)
      {
        upper_density = den_config (xplasma, ulvl);
        cooling_bf[i] = mplasma->cooling_bf[i] =
          upper_density * PLANCK * cont_ptr->freq[0] * (mplasma->recomb_sp_e[config[ulvl].bfd_indx_first + cont_ptr->down_index]);
      }
      else
      {
        upper_density = xplasma->density[cont_ptr->nion + 1];

        cooling_bf[i] = mplasma->cooling_bf[i] = upper_density * PLANCK * cont_ptr->freq[0] * (xplasma->recomb_simple[i]);
      }

      if (cooling_bf[i] < 0)
      {
        Error ("kpkt: phot %d bf cooling rate negative. Density was %g\n", p->np, upper_density);
        Error ("alpha_sp(cont_ptr, xplasma,2) %g \n", alpha_sp (cont_ptr, xplasma, 2));
        Error ("i, ulvl, nphot_total, nion %d %d %d %d\n", i, ulvl, nphot_total, cont_ptr->nion);
        Error ("nlev, z, istate %d %d %d \n", cont_ptr->nlev, cont_ptr->z, cont_ptr->istate);
        Error ("freq[0] %g\n", cont_ptr->freq[0]);
        cooling_bf[i] = mplasma->cooling_bf[i] = 0.0;
      }
      else
      {
        cooling_bftot += cooling_bf[i];
      }

      cooling_normalisation += cooling_bf[i];

      if (cont_ptr->macro_info == TRUE && geo.macro_simple == FALSE)
      {
        /* Include collisional ionization as a cooling term in macro atoms, but not simple atoms.  */

        lower_density = den_config (xplasma, cont_ptr->nlev);
        cooling_bf_col[i] = mplasma->cooling_bf_col[i] =
          lower_density * PLANCK * cont_ptr->freq[0] * q_ioniz (cont_ptr, electron_temperature);

        cooling_bf_coltot += cooling_bf_col[i];

        cooling_normalisation += cooling_bf_col[i];

      }

    }

    /* End of BF calculation and beginning of BB calculation.  Note that for macro atoms
       the upper level density is stored but for simple atoms it must be calculated. */

    for (i = 0; i < nlines; i++)
    {
      line_ptr = &line[i];
      if (line_ptr->macro_info == TRUE && geo.macro_simple == FALSE)
      {
        cooling_bb[i] = mplasma->cooling_bb[i] =
          den_config (xplasma, line_ptr->nconfigl) * q12 (line_ptr, electron_temperature) * line_ptr->freq * PLANCK;

      }
      else
      {
        two_level_atom (line_ptr, xplasma, &lower_density, &upper_density);

        coll_rate = q21 (line_ptr, electron_temperature) * (1. - exp (-H_OVER_K * line_ptr->freq / electron_temperature));

        cooling_bb[i] =
          (lower_density * line_ptr->gu / line_ptr->gl -
           upper_density) * coll_rate / (exp (H_OVER_K * line_ptr->freq / electron_temperature) - 1.) * line_ptr->freq * PLANCK;

        rad_rate = a21 (line_ptr) * p_escape (line_ptr, xplasma);

        /* Now multiply by the scattering probability - i.e. we are only going to consider bb cooling when
           the photon actually escapes - we don't to waste time by exciting a two-level macro atom only so that
           it makes another k-packet for us! (SS May 04) */

        cooling_bb[i] *= rad_rate / (rad_rate + (coll_rate * xplasma->ne));
        mplasma->cooling_bb[i] = cooling_bb[i];
      }

      if (cooling_bb[i] < 0)
      {
        cooling_bb[i] = mplasma->cooling_bb[i] = 0.0;
      }
      else
      {
        cooling_bbtot += cooling_bb[i];
      }
      cooling_normalisation += cooling_bb[i];
    }

    /* end of BB calculation  */


    if (one->inwind >= 0)
    {
      cooling_ff = mplasma->cooling_ff = total_free (one, xplasma->t_e, freqmin, freqmax) / xplasma->vol / xplasma->ne;
      cooling_ff += mplasma->cooling_ff_lofreq = total_free (one, xplasma->t_e, 0.0, freqmin) / xplasma->vol / xplasma->ne;
    }
    else
    {
      /* This should never happen */
      cooling_ff = mplasma->cooling_ff = mplasma->cooling_ff_lofreq = 0.0;
      Error ("kpkt: np %d A scattering event in cell %d with vol = 0???\n", p->np, one->nwind);
      *escape = TRUE;
      p->istat = P_ERROR_MATOM;
      return (0);
    }


    if (cooling_ff < 0)
    {
      Error ("kpkt: np %d ff cooling rate negative. Abort.", p->np);
      *escape = TRUE;
      p->istat = P_ERROR_MATOM;
      return (0);
    }
    else
    {
      cooling_normalisation += cooling_ff;
    }


    /* JM -- 1310 -- we now want to add adiabatic cooling as another way of destroying kpkts
       this should have already been calculated and stored in the plasma structure. Note that 
       adiabatic cooling does not depend on type of macro atom excited */

    /* note the units here- we divide the total luminosity of the cell by volume and ne to give cooling rate */

    cooling_adiabatic = xplasma->cool_adiabatic / xplasma->vol / xplasma->ne;   // JM 1411 - changed to use filled volume


    if (geo.adiabatic == 0 && cooling_adiabatic > 0.0)
    {
      Error ("kpkt: Adiabatic cooling turned off, but non zero in cell %d", xplasma->nplasma);
    }


    /* If the next error occurs see issue #70.  It is ghought to be fixed, with the possiple 
       exception of cells that are partially in the wind.
     */
    if (cooling_adiabatic < 0)
    {
      Error ("kpkt: Photon %d. Adiabatic cooling negative! Major problem if inwind (%d) == 0\n", p->np, one->inwind);
      Error ("kpkt: Photon %d. Setting adiabatic kpkt destruction probability to zero for this matom.\n", p->np);
      cooling_adiabatic = 0.0;
    }


    cooling_normalisation += cooling_adiabatic;

    mplasma->cooling_bbtot = cooling_bbtot;
    mplasma->cooling_bftot = cooling_bftot;
    mplasma->cooling_bf_coltot = cooling_bf_coltot;
    mplasma->cooling_adiabatic = cooling_adiabatic;
    mplasma->cooling_normalisation = cooling_normalisation;
    mplasma->kpkt_rates_known = 1;

  }

  return (0);
}




/**********************************************************/
/** 
 * @brief a routine which calculates what fraction of a level 
 *         emissivity comes out in a given frequency range
 *
 * @param [in]     WindPtr w   the ptr to the structure defining the wind
 * @param [in]     int upper   the upper level that we deactivate from
 * @param [in,out]  PhotPtr p   the packet at the point of activation and deactivation
 * @param [in,out]  int nres    the process by which deactivation occurs
 * @param [in]     fmin   minimum frequency of the band
 * @param [in]     fmax   maximum frequency of the band
 * @return 0
 *
 * @details similar to routines like emit_matom, except that we calculate the fraction
 * of emission in a band rather than calculating frequencies for photons. Used to be 
 * done via a less efficient rejection method.
 * 
***********************************************************/

double
f_matom_emit_accelerate (w, p, nres, upper, fmin, fmax)
     WindPtr w;
     PhotPtr p;
     int *nres;
     int upper;
     double fmin, fmax;
{
  struct lines *line_ptr;
  struct topbase_phot *cont_ptr;
  int uplvl;
  double eprbs[NBBJUMPS + NBFJUMPS], eprbs_band[NBBJUMPS + NBFJUMPS];
  double penorm, penorm_band;
  double threshold, run_tot;
  double sp_rec_rate;
  int n, m;
  int nbbd, nbfd;
  double t_e, ne;
  double bb_cont;
  WindPtr one;
  PlasmaPtr xplasma;
  double flast, fthresh, bf_int_full, bf_int_inrange;


  one = &w[p->grid];            //This is to identify the grid cell in which we are
  xplasma = &plasmamain[one->nplasma];

  t_e = xplasma->t_e;           //electron temperature 
  ne = xplasma->ne;             //electron number density


  /* The first step is to identify the configuration that has been excited. */

  uplvl = upper;

  /* Unlike the proper matom routine, we don't want any jumps, only deactivations here. */


  /*  The excited configuration is now known. Now compute all the probabilities of deactivation
     /jumping from this configuration. Then choose one. */


  nbbd = config[uplvl].n_bbd_jump;      //store these for easy access -- number of bb downward jumps
  nbfd = config[uplvl].n_bfd_jump;      // number of bf downared jumps from this transition


  // Start by setting everything to 0

  m = 0;                        //m counts the total number of possible ways to leave the level
  penorm = 0.0;                 //stores the total emission probability
  penorm_band = 0.0;

  for (n = 0; n < nbbd + nbfd; n++)
  {
    eprbs[n] = 0;               //stores the individual emission probabilities SS
    eprbs_band[n] = 0;
  }
  /* Finished zeroing. */


  /* bb downwards jumps */

  for (n = 0; n < nbbd; n++)
  {
    line_ptr = &line[config[uplvl].bbd_jump[n]];
    /* Since we are only interested in making an r-packet here we can (a) ignore collisional
       deactivation and (b) ignore lines outside the frequency range of interest. */
    bb_cont = (a21 (line_ptr) * p_escape (line_ptr, xplasma));

    eprbs[n] = bb_cont * (config[uplvl].ex - config[line[config[uplvl].bbd_jump[n]].nconfigl].ex);      //energy difference

    if (eprbs[n] < 0.)          //test (can be deleted eventually SS)
    {
      Error ("Negative probability (matom, 2). Abort.");
      Exit (0);
    }

    penorm += eprbs[n];
    if ((line_ptr->freq > fmin) && (line_ptr->freq < fmax))     // correct range
    {
      eprbs_band[m] = eprbs[n];
      penorm_band += eprbs_band[m];
      m++;
    }
  }

  /* bf downwards jumps */
  for (n = 0; n < nbfd; n++)
  {
    cont_ptr = &phot_top[config[uplvl].bfd_jump[n]];    //pointer to continuum

    /* If the edge is above the frequency range we are interested in then we need not consider this
       bf process. */
    sp_rec_rate = alpha_sp (cont_ptr, xplasma, 0);
    eprbs[n + nbbd] = sp_rec_rate * ne * (config[uplvl].ex - config[phot_top[config[uplvl].bfd_jump[n]].nlev].ex);      //energy difference
    if (eprbs[n + nbbd] < 0.)   //test (can be deleted eventually SS)
    {
      Error ("Negative probability (matom, 4). Abort.");
      Exit (0);
    }
    penorm += eprbs[n + nbbd];
    if (cont_ptr->freq[0] < fmax && cont_ptr->freq[cont_ptr->np - 1] > fmin)    //means that it may contribute
    {
      eprbs_band[m] = eprbs[n + nbbd];
      fthresh = cont_ptr->freq[0];      //first frequency in list
      flast = cont_ptr->freq[cont_ptr->np - 1]; //last frequency in list
      bf_int_full = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fthresh, flast);

      /* the limits differ depending on whether the band covers all or part of the cross-section. 
         four possibilities */
      if (fthresh < fmin && flast > fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fmin, fmax);
      }
      else if (fthresh < fmin && flast < fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fmin, flast);
      }
      else if (fthresh > fmin && flast > fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fthresh, fmax);
      }
      else if (fthresh > fmin && flast < fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fthresh, flast);
      }
      else
      {
        Error ("Something wrong here: f_matom_emit_accelerate broke the law!");
      }
      penorm_band += eprbs_band[m] * bf_int_inrange / bf_int_full;
      m++;
    }
  }

  /* catch the case where penorm == 0 for e.g. ground states with zero emission probability */
  if (penorm > 0)
  {
    return (penorm_band / penorm);
  }
  else
  {
    return (0.0);
  }
}

/* The frequency and the value of nres have been set correctly. All done. */






/**********************************************************/
/** 
 * @brief deals with the elimination of k-packets.
 *
 * Whenever a k-packet is created it is
 * immediately destroyed insitu by this routine. At output "nres" identified the process
 * that destroys the k-packet and the packet information has been updated in the same
 * way as at the end of matom
 *
 * @param [in]     WindPtr w   the ptr to the structure defining the wind
 * @param [in,out]  PhotPtr p   the packet at the point of activation and deactivation
 * @param [in,out]  int nres    the process which activates and deactivates the Macro Atom
 * @param [in,out]  int escape  to tell us whether the matom de-activation
 *                             is via an r-packet (1) or a k-packet (0)
 * @param [in] int mode         switch which allows photon to be deactivated by a non-radiative
 * term.  (non_zero is true)
 * @return 0
 *
 * ###Notes###
 *          Mar 04  SS   Coding began.
 *          Apr 04  SS   Various improvements including the addition of ff and collisions.
 *          May 04  SS   Minor changes made to collisional cooling rates (bug fixed)
 *                       and fb cooling for simple continua.
 *          May 04  SS   Modified to work for case with all "simple" ions.
 *          May 04  SS   Modified to use "scattering probability" formalism for 
 *                       simple ion cooling rates.
 *          Jun 04  SS   Modified to include the "escape" variable to identify excitation of
 *                       macro atoms. This removes the need to call matom from within this routine.
 *          Jun 04  SS   Modified to include collisional ionization as a cooling term.
 *          July04  SS   Modified to use recomb_sp(_e) rather than alpha_sp(_e) to speed up.
 *	06may	ksl	57+ -- Modified to use new plasma array.  Eliminated passing
 *			entire w array
 *	131030	JM 		-- Added adiabatic cooling as possible kpkt destruction choice 
 *	
 *	* 180616  Updated so that one could force kpkt to deactivate via radiation
************************************************************/

double
f_kpkt_emit_accelerate (p, nres, escape, mode, fmin, fmax)
     PhotPtr p;
     int *nres;
     int *escape;
     int mode;
     double fmin, fmax;
{

  int i;
  int ulvl, escape_dummy;
  double cooling_bf[nphot_total];
  double cooling_bf_col[nphot_total];   //collisional cooling in bf transitions
  double cooling_bb[NLINES];
  double cooling_adiabatic;
  struct topbase_phot *cont_ptr;
  struct lines *line_ptr;
  double cooling_normalisation;
  double destruction_choice;
  double electron_temperature;
  double cooling_bbtot, cooling_bftot, cooling_bf_coltot;
  double lower_density, upper_density;
  double cooling_ff, upweight_factor;
  WindPtr one;
  PlasmaPtr xplasma;
  MacroPtr mplasma;
  struct photon pdummy;

  double coll_rate, rad_rate;
  double freqmin, freqmax;
  double eprbs, eprbs_band, penorm, penorm_band;
  double flast, fthresh, bf_int_full, bf_int_inrange;



  penorm = 0.0;
  penorm_band = 0.0;

  one = &wmain[p->grid];
  xplasma = &plasmamain[one->nplasma];
  check_plasma (xplasma, "kpkt");
  mplasma = &macromain[xplasma->nplasma];

  electron_temperature = xplasma->t_e;

  /* JM 1511 -- Fix for issue 187. We need band limits for free free packet
     generation (see call to one_ff below) */
  freqmin = xband.f1[0];
  freqmax = ALPHA_FF * xplasma->t_e / H_OVER_K;

  /* ksl This is a Bandaid for when the temperatures are very low */
  /* in this case cooling_ff should be low compared to cooling_ff_lofreq anyway */
  if (freqmax < 1.1 * freqmin)
  {
    freqmax = 1.1 * freqmin;
  }


  /* Now need to do k-packet processes. This subroutine calculates the k-packet 
     cooling rates and stores them in mplasma->cooling. Dummy variables are needed
     because this routine is also used in the main kpkt routine */
  escape_dummy = 0;
  stuff_phot (p, &pdummy);
  fill_kpkt_rates (xplasma, &escape_dummy, &pdummy);

  for (i = 0; i < nphot_total; i++)
  {
    cont_ptr = &phot_top[i];    //pointer to continuum

    /* If the edge is above the frequency range we are interested in then we need not consider this
       bf process. */

    eprbs = mplasma->cooling_bf[i];
    penorm += eprbs;
    if (cont_ptr->freq[0] < fmax && cont_ptr->freq[cont_ptr->np - 1] > fmin)    //means that it may contribute
    {
      eprbs_band = mplasma->cooling_bf[i];
      fthresh = cont_ptr->freq[0];      //first frequency in list
      flast = cont_ptr->freq[cont_ptr->np - 1]; //last frequency in list
      bf_int_full = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fthresh, flast);
      if (fthresh < fmin && flast > fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fmin, fmax);
      }
      else if (fthresh < fmin && flast < fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fmin, flast);
      }
      else if (fthresh > fmin && flast > fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fthresh, fmax);
      }
      else if (fthresh > fmin && flast < fmax)
      {
        bf_int_inrange = scaled_alpha_sp_integral_band_limited (cont_ptr, xplasma, 0, fthresh, flast);
      }
      else
      {
        Error ("Something wrong here: f_matom_emit_accelerate broke the law!");
      }
      penorm_band += eprbs_band * bf_int_inrange / bf_int_full;
    }

    for (i = 0; i < nlines; i++)
    {
      if (line[i].macro_info == 1 && geo.macro_simple == 0)     //line is for a macro atom
      {
        eprbs = 0.0;            //these are not deactivations in this approach any more but jumps to macro atom levels
      }
      else                      //line is not for a macro atom - use simple method
      {
        penorm += eprbs = mplasma->cooling_bb[i];
        if ((line[i].freq > fmin) && (line[i].freq < fmax))     // correct range
        {
          penorm_band += eprbs_band = eprbs;
        }
      }
    }


    /* consult issues #187, #492 regarding free-free */
    penorm += eprbs = mplasma->cooling_ff + mplasma->cooling_ff_lofreq;
    if (fmin > freqmin)
    {
      penorm_band += total_free (one, xplasma->t_e, fmin, fmax) / total_free (one, xplasma->t_e, freqmin, freqmax) * mplasma->cooling_ff;
    }
    else if (fmax > freqmin)
    {
      penorm_band += total_free (one, xplasma->t_e, freqmin, fmax) / total_free (one, xplasma->t_e, freqmin, freqmax) * mplasma->cooling_ff;
      penorm_band +=
        total_free (one, xplasma->t_e, fmin, freqmin) / total_free (one, xplasma->t_e, 0, freqmin) * mplasma->cooling_ff_lofreq;
    }
    else
    {
      penorm_band += total_free (one, xplasma->t_e, fmin, fmax) / total_free (one, xplasma->t_e, 0, freqmin) * mplasma->cooling_ff_lofreq;
    }


    penorm += eprbs = mplasma->cooling_adiabatic;

    for (i = 0; i < nphot_total; i++)
    {
      penorm += eprbs = mplasma->cooling_bf_col[i];
    }
  }

  if (penorm > 0)
  {
    return (penorm_band / penorm);
  }
  else
  {
    return (0.0);
  }

}