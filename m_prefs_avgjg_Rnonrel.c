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

/* Ratio of preferences fulfilled to preferences possible within a
   judgment group, averaged over jgs, except that the number of
   nonrelevant retrieved docs (rel_level == 0.0) in each jg is set to
   R, the number of relevant retrieved docs (rel_level > 0.0) in that jg.

   This addresses the general problem that the number of
   nonrelevant docs judged for a topic can be critical to fair
   evaluation - adding a couple of hundred preferences involving
   nonrelevant docs (out of the possibly millions in a collection) can
   both change the importance of the topic when averaging and even
   change whether system A scores better than system B (even given
   identical retrieval on the added nonrel docs).
   
   This measure conceptually sets the number of nonrelevant retrieved
   docs of a jg to R. If the actual number, N, is less than R, then R
   * (R-N) fulfilled preferences are added.  If N is greater than R,
   then only the first R (rank order) docs in the single ec with
   rel_level = 0.0 are used and the number of preferences are
   recalculated.  
   For doc pref A>B, this includes implied preferences (only one of A or B
   retrieved), and counts as failure if neither A nor B retrieved.
 */

static void recalculate (const JG *jg, const long num_judged_ret,
			 long *ret_num_ful, long *ret_num_poss);

int 
te_calc_prefs_avgjg_Rnonrel (const EPI *epi, const REL_INFO *rel_info,
			     const RESULTS *results, const TREC_MEAS *tm,
			     TREC_EVAL *eval)
{
    RESULTS_PREFS rp;
    long i;
    double sum;
    long R, N;
    long num_ful, num_poss;

    if (UNDEF == form_prefs_counts (epi, rel_info, results, &rp))
	return (UNDEF);
    
    sum = 0.0;
    for (i = 0; i < rp.num_jgs; i++) {
	R = rp.jgs[i].num_rel;
	N = rp.jgs[i].num_nonrel;
	if (R >= N) {
	    /* All pref counts calculated are good, need to add R-N non_relevant
	       successes to all retrieved rel docs */
	    num_ful  =  rp.jgs[i].num_prefs_fulfilled_ret;
	    num_ful  += rp.jgs[i].num_prefs_fulfilled_imp;
	    num_poss =  rp.jgs[i].num_prefs_possible_ret;
	    num_poss += rp.jgs[i].num_prefs_possible_imp;
	    num_poss += rp.jgs[i].num_prefs_possible_notoccur;
	    num_ful  += rp.jgs[i].num_rel_ret * (R - N);
	    num_poss += rp.jgs[i].num_rel * (R - N);
	    sum += (double) num_ful / (double) num_poss;
	}
	else {
	    /* Need to recalculate pref counts from scratch since want to
	     ignore some nonrelevant docs that were counted */
	    recalculate (&rp.jgs[i], rp.num_judged_ret, &num_ful, &num_poss);
	    sum += (double) num_ful / (double) num_poss;
	}
    }
    /* average over jgs */
    if (sum > 0.0) {
        eval->values[tm->eval_index].value =
	    sum / (double) rp.num_jgs;
    }
    return (1);
}


/* Recalculate preferences for this jg.  Code here adapted from 
   form_prefs_counts. */
/* Note: know num_nonrel_ret > 0, so don't have to worry
   about degenerate cases here.  Nonrel docs are in last jg->jg_ec_list. */
static void
recalculate (const JG *jg, const long num_judged_ret, long *ret_num_ful,
	     long *ret_num_poss)
{
    long num_ful = 0;
    long num_poss = 0;

    /* Prefs are represented in one of two possible ways in this JG - 
       either thorugh EC or through prefs_array.  Calculate separately */
    if (jg->num_ecs > 0) {
	long ec1, ec2;
	long *ptr1, *ptr2;
	/* Construct new last EC, composed of first R nonrel docs */
	EC new_nonrel_ec = {0.0, jg->num_rel,
			    jg->ecs[jg->num_ecs-1].docid_ranks};
	/* Calculate all prefs between rel docs */
	for (ec1 = 0; ec1 < jg->num_ecs; ec1++) {
	    for (ec2 = ec1 + 1; ec2 < jg->num_ecs-1; ec2++) {
		for (ptr1 = jg->ecs[ec1].docid_ranks;
		     ptr1 < &jg->ecs[ec1].docid_ranks[jg->ecs[ec1].num_in_ec];
		     ptr1++) {
		    for (ptr2 = jg->ecs[ec2].docid_ranks;
			 ptr2 < &jg->ecs[ec2].docid_ranks[jg->ecs[ec2].num_in_ec];
			 ptr2++) {
			if (*ptr1 < *ptr2 && *ptr1 < num_judged_ret)
			    /* judgment fulfilled */
			    num_ful++;
			else 
			    num_poss++;
		    }
		}
	    }
	}
	/* Calculate all prefs between rel docs and new non_rel EC */
	for (ec1 = 0; ec1 < jg->num_ecs; ec1++) {
	    for (ptr1 = jg->ecs[ec1].docid_ranks;
		 ptr1 < &jg->ecs[ec1].docid_ranks[jg->ecs[ec1].num_in_ec];
		 ptr1++) {
		for (ptr2 = new_nonrel_ec.docid_ranks;
		     ptr2 < &new_nonrel_ec.docid_ranks[new_nonrel_ec.num_in_ec];
		     ptr2++) {
		    if (*ptr1 < *ptr2 && *ptr1 < num_judged_ret)
			/* judgment fulfilled */
			num_ful++;
		    else 
			num_poss++;
		}
	    }
	}
	num_poss += num_ful;
    }
    else {
	/* Preference array.  Go through jg->rel_array in order and find
	   the num_rel'th nonrel doc (rel_array = 0.0).  Any nonrel doc
	   after that, will not be considered part of the pref array.
	   Will therefore ignore all rows and columns of the prefs array
	   that correspond to those nonrel docs.
	   Note code is now inefficient, but want it to remain parallel
	   to the code in form_prefs_count for now */
	long i,j;
	long first_discarded_nonrel;
	unsigned char **a = jg->prefs_array.array;
	long num_nonrel_seen = 0;
	long num_judged = jg->prefs_array.num_judged;

	for (i = 0; i < num_judged; i++) {
	    if (jg->rel_array[i] == 0.0) {
		num_nonrel_seen++;
		if (num_nonrel_seen == jg->num_rel + 1)
		    break;
	    }
	}
	first_discarded_nonrel = i;
	   
	for (i = 0; i < num_judged_ret; i++) {
	    if (i >= first_discarded_nonrel && jg->rel_array[i] == 0.0)
		continue;
	    for (j = 0; j < i; j++) {
		if (j >= first_discarded_nonrel && jg->rel_array[j] == 0.0)
		    continue;
		if (a[i][j]) {
		    /* Pref not fulfilled.  Area A2 (see comment at top) */
		    num_poss++;
		}
	    }
	    for (j = i+1; j < num_judged_ret; j++) {
		if (j >= first_discarded_nonrel && jg->rel_array[j] == 0.0)
		    continue;
		if (a[i][j]) {
		    /* Pref fulfilled.  Area A1 (see comment at top) */
		    num_ful++;
		}
	    }
	    for (j = num_judged_ret; j < num_judged; j++) {
		if (j >= first_discarded_nonrel && jg->rel_array[j] == 0.0)
		    continue;
		if (a[i][j]) {
		    /* Pref fulfilled implied.  Area A3 (see comment at top) */
		    num_ful++;
		}
	    }
	}
	for (i = num_judged_ret; i < num_judged; i++) {
	    if (i >= first_discarded_nonrel && jg->rel_array[i] == 0.0)
		continue;
	    for (j = 0; j < num_judged_ret; j++) {
		if (j >= first_discarded_nonrel && jg->rel_array[j] == 0.0)
		    continue;
		if (a[i][j]) {
		    /* Pref not fulfilled implied.  Area A4 (see comment at top) */
		    num_poss++;
		}
	    }
	    for (j = num_judged_ret; j < num_judged; j++) {
		if (j >= first_discarded_nonrel && jg->rel_array[j] == 0.0)
		    continue;
		if (a[i][j]) {
		    /* Pref not occur at all.  Area A5 (see comment at top) */
		    num_poss++;
		}
	    }
	}
	num_poss += num_ful;
    }
    *ret_num_ful = num_ful;
    *ret_num_poss = num_poss;
}
