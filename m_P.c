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

/*       "    Precision at cutoffs\n\
    Precision measured at various doc level cutoffs in the ranking.\n\
    If the cutoff is larger than the number of docs retrieved, then\n\
    it is assumed nonrelevant docs fill in the rest.  Eg, if a method\n\
    retrieves 15 docs of which 4 are relevant, then P20 is 0.2 (4/20).\n\
    Precision is a very nice user oriented measure, and a good comparison\n\
    number for a single topic, but it does not average well. For example,\n\
    P20 has very different expected characteristics if there 300\n\
    total relevant docs for a topic as opposed to 10.\n\
    Note:   trec_eval -m P.50 ...\n\
    is different from \n\
            trec_eval -M 50 -m set_P ...\n\
    in that the latter will not fill in with nonrel docs if less than 50\n\
    docs retrieved\n\
    Cutoffs must be positive without duplicates\n\
    Default param: trec_eval -m P.5,10,15,20,30,100,200,500,1000\n",
*/

int 
te_calc_P (const EPI *epi, const REL_INFO *rel_info, const RESULTS *results,
	  const TREC_MEAS *tm, TREC_EVAL *eval)
{
    LONG_PARAMS *cutoffs = (LONG_PARAMS *) tm->meas_params;
    long cutoff_index = 0;
    long i;
    RANK_REL rank_rel;
    long rel_so_far = 0;

    if (UNDEF == form_ordered_rel (epi, rel_info, results, &rank_rel))
	return (UNDEF);

    for (i = 0; i < rank_rel.num_ret; i++) {
	if (i == cutoffs->param_values[cutoff_index]) {
	    /* Calculate previous cutoff threshold.
	       Note all guaranteed to be positive by init_meas */
	    eval->values[tm->eval_index+cutoff_index].value =
		(double) rel_so_far / (double) i;
	    if (++cutoff_index == cutoffs->num_params)
		break;
	}
	if (rank_rel.results_rel_list[i] >= epi->relevance_level)
	    rel_so_far++;
    }
    /* calculate values for those cutoffs not achieved */
    while (cutoff_index < cutoffs->num_params) {
	eval->values[tm->eval_index+cutoff_index].value =
	    (double) rel_so_far / (double) cutoffs->param_values[cutoff_index];
	cutoff_index++;
    }
    return (1);
}
