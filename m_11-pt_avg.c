#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/trvec_trec_eval.c,v 11.0 1992/07/21 18:20:35 chrisb Exp chrisb $";
#endif

/* Copyright (c) 2008
*/

#include "common.h"
#include "sysfunc.h"
#include "trec_eval.h"
#include "functions.h"
#include "trec_format.h"

/*      "    Interpolated Precision averaged over 11 recall points\n\
    Obsolete, only use for comparisons of old runs; should use map instead.\n\
    Average interpolated at the given recall points - default is the\n\
    11 points being reported for ircl_prn.\n\
    Both map and 11-pt_avg (and even R-prec) can be regarded as estimates of\n\
    the area under the standard ircl_prn curve.\n\
    Warning: name assumes user does not change default parameter values:\n\
    measure name is independent of parameter values and number of parameters.\n\
    Will actually average over all parameter values given.\n\
    To get 3-pt_avg as in trec_eval version 8 and earlier, use\n\
      trec_eval -m 11-pt_avg.0.2,0.5,0.8 ...\n\
    Default usage: trec_eval -m 11-pt_avg.0.0,.1,.2,.3,.4,.5,.6,.7,.8..9,1.0\n",
*/

int 
te_calc_11ptavg (const EPI *epi, const REL_INFO *rel_info,
		 const RESULTS *results, const TREC_MEAS *tm, TREC_EVAL *eval)
{
    FLOAT_PARAMS *cutoff_percents = (FLOAT_PARAMS *) tm->meas_params;
    long *cutoffs;    /* cutoffs expressed in num rel docs instead of percents*/
    long current_cut; /* current index into cutoffs */
    RANK_REL rr;
    long rel_so_far;
    long i;
    double precis, int_precis;
    double sum = 0.0;

    if (0 == cutoff_percents->num_params) {
	fprintf (stderr, "trec_eval.calc_m_11ptavg: No cutoff values\n");
	return (UNDEF);
    }

    if (UNDEF == form_ordered_rel (epi, rel_info, results, &rr))
	return (UNDEF);

    /* translate percentage of rels as given in the measure params, to
       an actual cutoff number of docs.  Note addition of 0.9 
       means the default 11 percentages should have same cutoffs as
       historical MAP implementations (eg, old trec_eval) */
    if (NULL == (cutoffs = Malloc (cutoff_percents->num_params, long)))
	return (UNDEF);
    for (i = 0; i < cutoff_percents->num_params; i++)
	cutoffs[i] = (long) (cutoff_percents->param_values[i] * rr.num_rel+0.9);

    current_cut = cutoff_percents->num_params - 1;
    while (current_cut > 0 && cutoffs[current_cut] > rr.num_rel_ret)
	current_cut--;

    /* Loop over all retrieved docs in reverse order.  Needs to be
       reverse order since are calcualting interpolated precision.
       Int_Prec (X) defined to be MAX (Prec (Y)) for all Y >= X. */
    precis = (double) rr.num_rel_ret / (double) rr.num_ret;
    int_precis = precis;
    rel_so_far = rr.num_rel_ret;
    for (i = rr.num_ret; i > 0 && rel_so_far > 0; i--) {
	precis = (double) rel_so_far / (double) i;
	if (int_precis < precis)
	    int_precis = precis;
	if (rr.results_rel_list[i-1] >= epi->relevance_level) {
            while (current_cut >= 0 && rel_so_far == cutoffs[current_cut]) {
		sum += int_precis;
                current_cut--;
            }
            rel_so_far--;
	}
    }

    while (current_cut >= 0) {
	sum += int_precis;
	current_cut--;
    }

    eval->values[tm->eval_index].value =
	sum / (double) cutoff_percents->num_params;
    (void) Free (cutoffs);

    return (1);
}
