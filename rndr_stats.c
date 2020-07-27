/*
 * rndr_stats.c: Funtions used by sadf to display statistics in selected format.
 * (C) 1999-2020 by Sebastien GODARD (sysstat <at> orange.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "sa.h"
#include "ioconf.h"
#include "rndr_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

char *seps[] =  {"\t", ";"};

extern unsigned int flags;

/*
 ***************************************************************************
 * cons() -
 *   encapsulate a pair of ints or pair of char * into a static Cons and
 *   return a pointer to it.
 *
 * given:   t - type of Cons {iv, sv}
 *	    arg1 - unsigned long int (if iv), char * (if sv) to become
 *		   element 'a'
 *	    arg2 - unsigned long int (if iv), char * (if sv) to become
 *		   element 'b'
 *
 * does:    load a static Cons with values using the t parameter to
 *	    guide pulling values from the arglist
 *
 * return:  the address of its static Cons.  If you need to keep
 *	    the contents of this Cons, copy it somewhere before calling
 *	    cons() against to avoid overwrite.
 *	    ie. don't do this:  f( cons( iv, i, j ), cons( iv, a, b ) );
 ***************************************************************************
 */
static Cons *cons(tcons t, ...)
{
	va_list ap;
	static Cons c;

	c.t = t;

	va_start(ap, t);
	if (t == iv) {
		c.a.i = va_arg(ap, unsigned long int);
		c.b.i = va_arg(ap, unsigned long int);
	}
	else {
		c.a.s = va_arg(ap, char *);
		c.b.s = va_arg(ap, char *);
	}
	va_end(ap);
	return(&c);
}

/*
 ***************************************************************************
 * render():
 *
 * given:    isdb - flag, true if db printing, false if ppc printing
 *	     pre  - prefix string for output entries
 *	     rflags - PT_.... rendering flags
 *	     pptxt - printf-format text required for ppc output (may be null)
 *	     dbtxt - printf-format text required for db output (may be null)
 *	     mid - pptxt/dbtxt format args as a Cons.
 *	     lluval - %llu printable arg (PT_USEINT must be set)
 *	     dval  - %.2f printable arg (used unless PT_USEINT is set)
 *	     sval - %s printable arg (PT_USESTR must be set)
 *
 * does:     print [pre<sep>]([dbtxt,arg,arg<sep>]|[pptxt,arg,arg<sep>]) \
 *                     (luval|dval)(<sep>|\n)
 *
 * return:   void.
 ***************************************************************************
 */
static void render(int isdb, char *pre, int rflags, const char *pptxt,
		   const char *dbtxt, Cons *mid, unsigned long long lluval,
		   double dval, char *sval)
{
	static int newline = 1;
	const char *txt[]  = {pptxt, dbtxt};

	/* Start a new line? */
	if (newline && !DISPLAY_HORIZONTALLY(flags)) {
		printf("%s", pre);
	}

	/* Terminate this one ? ppc always gets a newline */
	newline = ((rflags & PT_NEWLIN) || !isdb);

	if (txt[isdb]) {
		/* pp/dbtxt? */

		printf("%s", seps[isdb]);	/* Only if something actually gets printed */

		if (mid) {
			/* Got format args? */
			switch(mid->t) {
			case iv:
				printf(txt[isdb], mid->a.i, mid->b.i);
				break;
			case sv:
				printf(txt[isdb], mid->a.s, mid->b.s);
				break;
			}
		}
		else {
			printf("%s", txt[isdb]);
		}
	}

	if (rflags & PT_USEINT) {
		printf("%s%llu", seps[isdb], lluval);
	}
	else if (rflags & PT_USESTR) {
		printf("%s%s", seps[isdb], sval);
	}
	else if (rflags & PT_USERND) {
		printf("%s%.0f", seps[isdb], dval);
	}
	else {
		printf("%s%.2f", seps[isdb], dval);
	}
	if (newline) {
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display CPU statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second (independent of the
 *		number of processors). Unused here.
 ***************************************************************************
 */
__print_funct_t render_cpu_stats(struct activity *a, int isdb, char *pre,
				 int curr, unsigned long long itv)
{
	int i;
	unsigned long long deltot_jiffies = 1;
	struct stats_cpu *scc, *scp;
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	/* @nr[curr] cannot normally be greater than @nr_ini */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/*
	 * Compute CPU "all" as sum of all individual CPU (on SMP machines)
	 * and look for offline CPU.
	 */
	if (a->nr_ini > 1) {
		deltot_jiffies = get_global_cpu_statistics(a, !curr, curr,
							   flags, offline_cpu_bitmap);
	}

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* Don't display CPU */
			continue;

		scc = (struct stats_cpu *) ((char *) a->buf[curr]  + i * a->msize);
		scp = (struct stats_cpu *) ((char *) a->buf[!curr] + i * a->msize);

		if (i == 0) {
			/* This is CPU "all" */
			if (a->nr_ini == 1) {
				/*
				 * This is a UP machine. In this case
				 * interval has still not been calculated.
				 */
				deltot_jiffies = get_per_cpu_interval(scc, scp);
			}
			if (!deltot_jiffies) {
				/* CPU "all" cannot be tickless */
				deltot_jiffies = 1;
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				render(isdb, pre,
				       PT_NOFLAG,	/* that's zero but you know what it means */
				       "all\t%user",	/* ppctext */
				       "-1",		/* look! dbtext */
				       NULL,		/* no args */
				       NOVAL,		/* another 0, named for readability */
				       ll_sp_value(scp->cpu_user, scc->cpu_user, deltot_jiffies),
				       NULL);		/* No string arg */
			}
			else if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "all\t%usr", "-1", NULL,
				       NOVAL,
				       (scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest) ?
				       0.0 :
				       ll_sp_value(scp->cpu_user - scp->cpu_guest,
						   scc->cpu_user - scc->cpu_guest,
						   deltot_jiffies),
				       NULL);
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "all\t%nice", NULL, NULL,
				       NOVAL,
				       ll_sp_value(scp->cpu_nice, scc->cpu_nice, deltot_jiffies),
				       NULL);
			}
			else if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "all\t%nice", NULL, NULL,
				       NOVAL,
				       (scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice) ?
				       0.0 :
				       ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
						   scc->cpu_nice - scc->cpu_guest_nice,
						   deltot_jiffies),
				       NULL);
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "all\t%system", NULL, NULL,
				       NOVAL,
				       ll_sp_value(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
						   scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq,
						   deltot_jiffies),
				       NULL);
			}
			else if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "all\t%sys", NULL, NULL,
				       NOVAL,
				       ll_sp_value(scp->cpu_sys, scc->cpu_sys, deltot_jiffies),
				       NULL);
			}

			render(isdb, pre, PT_NOFLAG,
			       "all\t%iowait", NULL, NULL,
			       NOVAL,
			       ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, deltot_jiffies),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "all\t%steal", NULL, NULL,
			       NOVAL,
			       ll_sp_value(scp->cpu_steal, scc->cpu_steal, deltot_jiffies),
			       NULL);

			if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "all\t%irq", NULL, NULL,
				       NOVAL,
				       ll_sp_value(scp->cpu_hardirq, scc->cpu_hardirq, deltot_jiffies),
				       NULL);

				render(isdb, pre, PT_NOFLAG,
				       "all\t%soft", NULL, NULL,
				       NOVAL,
				       ll_sp_value(scp->cpu_softirq, scc->cpu_softirq, deltot_jiffies),
				       NULL);

				render(isdb, pre, PT_NOFLAG,
				       "all\t%guest", NULL, NULL,
				       NOVAL,
				       ll_sp_value(scp->cpu_guest, scc->cpu_guest, deltot_jiffies),
				       NULL);

				render(isdb, pre, PT_NOFLAG,
				       "all\t%gnice", NULL, NULL,
				       NOVAL,
				       ll_sp_value(scp->cpu_guest_nice, scc->cpu_guest_nice, deltot_jiffies),
				       NULL);
			}

			render(isdb, pre, pt_newlin,
			       "all\t%idle", NULL, NULL,
			       NOVAL,
			       (scc->cpu_idle < scp->cpu_idle) ?
			       0.0 :
			       ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies),
			       NULL);
		}
		else {
			/*
			 * Recalculate itv for current proc.
			 * If the result is 0, then current CPU is a tickless one.
			 */
			deltot_jiffies = get_per_cpu_interval(scc, scp);

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%user",		/* ppc text with formatting */
				       "%d",			/* db text with format char */
				       cons(iv, i - 1, NOVAL),	/* how we pass format args  */
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :			/* CPU is tickless */
				       ll_sp_value(scp->cpu_user, scc->cpu_user, deltot_jiffies),
				       NULL);
			}
			else if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%usr", "%d", cons(iv, i - 1, NOVAL),
				       NOVAL,
				       (!deltot_jiffies ||
				       ((scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest))) ?
				       0.0 :
				       ll_sp_value(scp->cpu_user - scp->cpu_guest,
						   scc->cpu_user - scc->cpu_guest, deltot_jiffies),
				       NULL);
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%nice", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :
				       ll_sp_value(scp->cpu_nice, scc->cpu_nice, deltot_jiffies),
				       NULL);
			}
			else if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%nice", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       (!deltot_jiffies ||
				       ((scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice))) ?
				       0.0 :
				       ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
						   scc->cpu_nice - scc->cpu_guest_nice, deltot_jiffies),
				       NULL);
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%system", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :
				       ll_sp_value(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
						   scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq,
						   deltot_jiffies),
				       NULL);
			}
			else if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%sys", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :
				       ll_sp_value(scp->cpu_sys, scc->cpu_sys, deltot_jiffies),
				       NULL);
			}

			render(isdb, pre, PT_NOFLAG,
			       "cpu%d\t%%iowait", NULL, cons(iv, i - 1, NOVAL),
			       NOVAL,
			       !deltot_jiffies ?
			       0.0 :
			       ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, deltot_jiffies),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "cpu%d\t%%steal", NULL, cons(iv, i - 1, NOVAL),
			       NOVAL,
			       !deltot_jiffies ?
			       0.0 :
			       ll_sp_value(scp->cpu_steal, scc->cpu_steal, deltot_jiffies),
			       NULL);

			if (DISPLAY_CPU_ALL(a->opt_flags)) {
				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%irq", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :
				       ll_sp_value(scp->cpu_hardirq, scc->cpu_hardirq, deltot_jiffies),
				       NULL);

				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%soft", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :
				       ll_sp_value(scp->cpu_softirq, scc->cpu_softirq, deltot_jiffies),
				       NULL);

				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%guest", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :
				       ll_sp_value(scp->cpu_guest, scc->cpu_guest, deltot_jiffies),
				       NULL);

				render(isdb, pre, PT_NOFLAG,
				       "cpu%d\t%%gnice", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       !deltot_jiffies ?
				       0.0 :
				       ll_sp_value(scp->cpu_guest_nice, scc->cpu_guest_nice, deltot_jiffies),
				       NULL);
			}

			if (!deltot_jiffies) {
				/* CPU is tickless */
				render(isdb, pre, pt_newlin,
				       "cpu%d\t%%idle", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       100.0,
				       NULL);
			}
			else {
				render(isdb, pre, pt_newlin,
				       "cpu%d\t%%idle", NULL, cons(iv, i - 1, NOVAL),
				       NOVAL,
				       (scc->cpu_idle < scp->cpu_idle) ?
				       0.0 :
				       ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies),
				       NULL);
			}
		}
	}
}

/*
 ***************************************************************************
 * Display task creation and context switch statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_pcsw_stats(struct activity *a, int isdb, char *pre,
				  int curr, unsigned long long itv)
{
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr],
		*spp = (struct stats_pcsw *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	/* The first one as an example */
	render(isdb,		/* db/ppc flag */
	       pre,		/* the preformatted line leader */
	       PT_NOFLAG,	/* is this the end of a db line? */
	       "-\tproc/s",	/* ppc text */
	       NULL,		/* db text */
	       NULL,		/* db/ppc text format args (Cons *) */
	       NOVAL,		/* %lu value (unused unless PT_USEINT) */
	       /* and %.2f value, used unless PT_USEINT */
	       S_VALUE(spp->processes, spc->processes, itv),
	       NULL);		/* %s value */

	render(isdb, pre, pt_newlin,
	       "-\tcswch/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->context_switch, spc->context_switch, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display interrupts statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_irq_stats(struct activity *a, int isdb, char *pre,
				 int curr, unsigned long long itv)
{
	int i;
	struct stats_irq *sic, *sip;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		sic = (struct stats_irq *) ((char *) a->buf[curr]  + i * a->msize);
		sip = (struct stats_irq *) ((char *) a->buf[!curr] + i * a->msize);

		/* Should current interrupt (including int "sum") be displayed? */
		if (a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) {

			/* Yes: Display it */
			if (!i) {
				/* This is interrupt "sum" */
				render(isdb, pre, pt_newlin,
				       "sum\tintr/s", "-1", NULL,
				       NOVAL,
				       S_VALUE(sip->irq_nr, sic->irq_nr, itv),
				       NULL);
			}
			else {
				render(isdb, pre, pt_newlin,
				       "i%03d\tintr/s", "%d", cons(iv, i - 1, NOVAL),
				       NOVAL,
				       S_VALUE(sip->irq_nr, sic->irq_nr, itv),
				       NULL);
			}
		}
	}
}

/*
 ***************************************************************************
 * Display swapping statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_swap_stats(struct activity *a, int isdb, char *pre,
				  int curr, unsigned long long itv)
{
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr],
		*ssp = (struct stats_swap *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpswpin/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(ssp->pswpin, ssc->pswpin, itv),
	       NULL);
	render(isdb, pre, pt_newlin,
	       "-\tpswpout/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(ssp->pswpout, ssc->pswpout, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display paging statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_paging_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr],
		*spp = (struct stats_paging *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpgpgin/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgpgin, spc->pgpgin, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpgpgout/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgpgout, spc->pgpgout, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tfault/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgfault, spc->pgfault, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tmajflt/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgmajfault, spc->pgmajfault, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpgfree/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgfree, spc->pgfree, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpgscank/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgscan_kswapd, spc->pgscan_kswapd, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpgscand/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgscan_direct, spc->pgscan_direct, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpgsteal/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(spp->pgsteal, spc->pgsteal, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\t%vmeff", NULL, NULL,
	       NOVAL,
	       (spc->pgscan_kswapd + spc->pgscan_direct -
		spp->pgscan_kswapd - spp->pgscan_direct) ?
	       SP_VALUE(spp->pgsteal, spc->pgsteal,
			spc->pgscan_kswapd + spc->pgscan_direct -
			spp->pgscan_kswapd - spp->pgscan_direct) : 0.0,
	       NULL);
}

/*
 ***************************************************************************
 * Display I/O and transfer rate statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_io_stats(struct activity *a, int isdb, char *pre,
				int curr, unsigned long long itv)
{
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr],
		*sip = (struct stats_io *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	/*
	 * If we get negative values, this is probably because
	 * one or more devices/filesystems have been unmounted.
	 * We display 0.0 in this case though we should rather tell
	 * the user that the value cannot be calculated here.
	 */
	render(isdb, pre, PT_NOFLAG,
	       "-\ttps", NULL, NULL,
	       NOVAL,
	       sic->dk_drive < sip->dk_drive ? 0.0 :
	       S_VALUE(sip->dk_drive, sic->dk_drive, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\trtps", NULL, NULL,
	       NOVAL,
	       sic->dk_drive_rio < sip->dk_drive_rio ? 0.0 :
	       S_VALUE(sip->dk_drive_rio, sic->dk_drive_rio, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\twtps", NULL, NULL,
	       NOVAL,
	       sic->dk_drive_wio < sip->dk_drive_wio ? 0.0 :
	       S_VALUE(sip->dk_drive_wio, sic->dk_drive_wio, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tdtps", NULL, NULL,
	       NOVAL,
	       sic->dk_drive_dio < sip->dk_drive_dio ? 0.0 :
	       S_VALUE(sip->dk_drive_dio, sic->dk_drive_dio, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tbread/s", NULL, NULL,
	       NOVAL,
	       sic->dk_drive_rblk < sip->dk_drive_rblk ? 0.0 :
	       S_VALUE(sip->dk_drive_rblk, sic->dk_drive_rblk, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tbwrtn/s", NULL, NULL,
	       NOVAL,
	       sic->dk_drive_wblk < sip->dk_drive_wblk ? 0.0 :
	       S_VALUE(sip->dk_drive_wblk, sic->dk_drive_wblk, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tbdscd/s", NULL, NULL,
	       NOVAL,
	       sic->dk_drive_dblk < sip->dk_drive_dblk ? 0.0 :
	       S_VALUE(sip->dk_drive_dblk, sic->dk_drive_dblk, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display memory and swap statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_memory_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);
	int ptn;
	unsigned long long nousedmem;

	if (DISPLAY_MEMORY(a->opt_flags)) {

		nousedmem = smc->frmkb + smc->bufkb + smc->camkb + smc->slabkb;
		if (nousedmem > smc->tlmkb) {
			nousedmem = smc->tlmkb;
		}

		render(isdb, pre, PT_USEINT,
		       "-\tkbmemfree", NULL, NULL,
		       smc->frmkb, DNOVAL, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbavail", NULL, NULL,
		       smc->availablekb, DNOVAL, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbmemused", NULL, NULL,
		       smc->tlmkb - nousedmem, DNOVAL, NULL);

		render(isdb, pre, PT_NOFLAG,
		       "-\t%memused", NULL, NULL, NOVAL,
		       smc->tlmkb ?
		       SP_VALUE(nousedmem, smc->tlmkb, smc->tlmkb) :
		       0.0, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbbuffers", NULL, NULL,
		       smc->bufkb, DNOVAL, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbcached", NULL, NULL,
		       smc->camkb, DNOVAL, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbcommit", NULL, NULL,
		       smc->comkb, DNOVAL, NULL);

		render(isdb, pre, PT_NOFLAG,
		       "-\t%commit", NULL, NULL, NOVAL,
		       (smc->tlmkb + smc->tlskb) ?
		       SP_VALUE(0, smc->comkb, smc->tlmkb + smc->tlskb) :
		       0.0, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbactive", NULL, NULL,
		       smc->activekb, DNOVAL, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbinact", NULL, NULL,
		       smc->inactkb, DNOVAL, NULL);

		ptn = DISPLAY_MEM_ALL(a->opt_flags) ? 0 : pt_newlin;
		render(isdb, pre, PT_USEINT | ptn,
		       "-\tkbdirty", NULL, NULL,
		       smc->dirtykb, DNOVAL, NULL);

		if (DISPLAY_MEM_ALL(a->opt_flags)) {
			render(isdb, pre, PT_USEINT,
			       "-\tkbanonpg", NULL, NULL,
			       smc->anonpgkb, DNOVAL, NULL);

			render(isdb, pre, PT_USEINT,
			       "-\tkbslab", NULL, NULL,
			       smc->slabkb, DNOVAL, NULL);

			render(isdb, pre, PT_USEINT,
			       "-\tkbkstack", NULL, NULL,
			       smc->kstackkb, DNOVAL, NULL);

			render(isdb, pre, PT_USEINT,
			       "-\tkbpgtbl", NULL, NULL,
			       smc->pgtblkb, DNOVAL, NULL);

			render(isdb, pre, PT_USEINT | pt_newlin,
			       "-\tkbvmused", NULL, NULL,
			       smc->vmusedkb, DNOVAL, NULL);
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		render(isdb, pre, PT_USEINT,
		       "-\tkbswpfree", NULL, NULL,
		       smc->frskb, DNOVAL, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbswpused", NULL, NULL,
		       smc->tlskb - smc->frskb, DNOVAL, NULL);

		render(isdb, pre, PT_NOFLAG,
		       "-\t%swpused", NULL, NULL, NOVAL,
		       smc->tlskb ?
		       SP_VALUE(smc->frskb, smc->tlskb, smc->tlskb) :
		       0.0, NULL);

		render(isdb, pre, PT_USEINT,
		       "-\tkbswpcad", NULL, NULL,
		       smc->caskb, DNOVAL, NULL);

		render(isdb, pre, pt_newlin,
		       "-\t%swpcad", NULL, NULL, NOVAL,
		       (smc->tlskb - smc->frskb) ?
		       SP_VALUE(0, smc->caskb, smc->tlskb - smc->frskb) :
		       0.0, NULL);
	}
}

/*
 ***************************************************************************
 * Display kernel tables statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_ktables_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_USEINT,
	       "-\tdentunusd", NULL, NULL,
	       skc->dentry_stat, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tfile-nr", NULL, NULL,
	       skc->file_used, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tinode-nr", NULL, NULL,
	       skc->inode_used, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT | pt_newlin,
	       "-\tpty-nr", NULL, NULL,
	       skc->pty_nr, DNOVAL, NULL);
}

/*
 ***************************************************************************
 * Display queue and load statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_queue_stats(struct activity *a, int isdb, char *pre,
				   int curr, unsigned long long itv)
{
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_USEINT,
	       "-\trunq-sz", NULL, NULL,
	       sqc->nr_running, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tplist-sz", NULL, NULL,
	       sqc->nr_threads, DNOVAL, NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tldavg-1", NULL, NULL,
	       NOVAL,
	       (double) sqc->load_avg_1 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tldavg-5", NULL, NULL,
	       NOVAL,
	       (double) sqc->load_avg_5 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tldavg-15", NULL, NULL,
	       NOVAL,
	       (double) sqc->load_avg_15 / 100,
	       NULL);

	render(isdb, pre, PT_USEINT | pt_newlin,
	       "-\tblocked", NULL, NULL,
	       sqc->procs_blocked, DNOVAL, NULL);
}

/*
 ***************************************************************************
 * Display serial lines statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_serial_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	int i, j, j0, found;
	struct stats_serial *ssc, *ssp;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	for (i = 0; i < a->nr[curr]; i++) {

		found = FALSE;

		if (a->nr[!curr] > 0) {
			ssc = (struct stats_serial *) ((char *) a->buf[curr]  + i * a->msize);

			/* Look for corresponding serial line in previous iteration */
			j = i;

			if (j >= a->nr[!curr]) {
				j = a->nr[!curr] - 1;
			}

			j0 = j;

			do {
				ssp = (struct stats_serial *) ((char *) a->buf[!curr] + j * a->msize);
				if (ssc->line == ssp->line) {
					found = TRUE;
					break;
				}
				if (++j >= a->nr[!curr]) {
					j = 0;
				}
			}
			while (j != j0);
		}

		if (!found)
			continue;

		render(isdb, pre, PT_NOFLAG,
		       "ttyS%d\trcvin/s", "%d",
		       cons(iv, ssc->line, NOVAL),
		       NOVAL,
		       S_VALUE(ssp->rx, ssc->rx, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "ttyS%d\txmtin/s", NULL,
		       cons(iv, ssc->line, NOVAL),
		       NOVAL,
		       S_VALUE(ssp->tx, ssc->tx, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "ttyS%d\tframerr/s", NULL,
		       cons(iv, ssc->line, NOVAL),
		       NOVAL,
		       S_VALUE(ssp->frame, ssc->frame, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "ttyS%d\tprtyerr/s", NULL,
		       cons(iv, ssc->line, NOVAL),
		       NOVAL,
		       S_VALUE(ssp->parity, ssc->parity, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "ttyS%d\tbrk/s", NULL,
		       cons(iv, ssc->line, NOVAL),
		       NOVAL,
		       S_VALUE(ssp->brk, ssc->brk, itv),
		       NULL);

		render(isdb, pre, pt_newlin,
		       "ttyS%d\tovrun/s", NULL,
		       cons(iv, ssc->line, NOVAL),
		       NOVAL,
		       S_VALUE(ssp->overrun, ssc->overrun, itv),
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display disks statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_disk_stats(struct activity *a, int isdb, char *pre,
				  int curr, unsigned long long itv)
{
	int i, j;
	struct stats_disk *sdc,	*sdp, sdpzero;
	struct ext_disk_stats xds;
	char *dev_name;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	memset(&sdpzero, 0, STATS_DISK_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		sdc = (struct stats_disk *) ((char *) a->buf[curr] + i * a->msize);

		j = check_disk_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sdp = &sdpzero;
		}
		else {
			sdp = (struct stats_disk *) ((char *) a->buf[!curr] + j * a->msize);
		}

		/* Get device name */
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn, sdc->part_nr,
					   DISPLAY_PRETTY(flags), DISPLAY_PERSIST_NAME_S(flags),
					   USE_STABLE_ID(flags), NULL);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, dev_name))
				/* Device not found */
				continue;
		}

		/* Compute extended stats (service time, etc.) */
		compute_ext_disk_stats(sdc, sdp, itv, &xds);

		render(isdb, pre, PT_NOFLAG,
		       "%s\ttps", "%s",
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       S_VALUE(sdp->nr_ios, sdc->nr_ios, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\trkB/s", NULL,
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       S_VALUE(sdp->rd_sect, sdc->rd_sect, itv) / 2,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\twkB/s", NULL,
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       S_VALUE(sdp->wr_sect, sdc->wr_sect, itv) / 2,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\tdkB/s", NULL,
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       S_VALUE(sdp->dc_sect, sdc->dc_sect, itv) / 2,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\tareq-sz", NULL,
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       xds.arqsz / 2,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\taqu-sz", NULL,
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       S_VALUE(sdp->rq_ticks, sdc->rq_ticks, itv) / 1000.0,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\tawait", NULL,
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       xds.await,
		       NULL);

		render(isdb, pre, pt_newlin,
		       "%s\t%%util", NULL,
		       cons(sv, dev_name, NULL),
		       NOVAL,
		       xds.util / 10.0,
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display network interfaces statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_dev_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	int i, j;
	struct stats_net_dev *sndc, *sndp, sndzero;
	double rxkb, txkb, ifutil;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	memset(&sndzero, 0, STATS_NET_DEV_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, sndc->interface))
				/* Device not found */
				continue;
		}

		j = check_net_dev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sndp = &sndzero;
		}
		else {
			sndp = (struct stats_net_dev *) ((char *) a->buf[!curr] + j * a->msize);
		}

		render(isdb, pre, PT_NOFLAG,
		       "%s\trxpck/s", "%s",
		       cons(sv, sndc->interface, NULL), /* What if the format args are strings? */
		       NOVAL,
		       S_VALUE(sndp->rx_packets, sndc->rx_packets, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\ttxpck/s", NULL,
		       cons(sv, sndc->interface, NULL),
		       NOVAL,
		       S_VALUE(sndp->tx_packets, sndc->tx_packets, itv),
		       NULL);

		rxkb = S_VALUE(sndp->rx_bytes, sndc->rx_bytes, itv);
		render(isdb, pre, PT_NOFLAG,
		       "%s\trxkB/s", NULL,
		       cons(sv, sndc->interface, NULL),
		       NOVAL,
		       rxkb / 1024,
		       NULL);

		txkb = S_VALUE(sndp->tx_bytes, sndc->tx_bytes, itv);
		render(isdb, pre, PT_NOFLAG,
		       "%s\ttxkB/s", NULL,
		       cons(sv, sndc->interface, NULL),
		       NOVAL,
		       txkb / 1024,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\trxcmp/s", NULL,
		       cons(sv, sndc->interface, NULL),
		       NOVAL,
		       S_VALUE(sndp->rx_compressed, sndc->rx_compressed, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\ttxcmp/s", NULL,
		       cons(sv, sndc->interface, NULL),
		       NOVAL,
		       S_VALUE(sndp->tx_compressed, sndc->tx_compressed, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\trxmcst/s", NULL,
		       cons(sv, sndc->interface, NULL),
		       NOVAL,
		       S_VALUE(sndp->multicast, sndc->multicast, itv),
		       NULL);

		ifutil = compute_ifutil(sndc, rxkb, txkb);
		render(isdb, pre, pt_newlin,
		       "%s\t%%ifutil", NULL,
		       cons(sv, sndc->interface, NULL),
		       NOVAL,
		       ifutil,
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display network interface errors statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_edev_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	int i, j;
	struct stats_net_edev *snedc, *snedp, snedzero;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	memset(&snedzero, 0, STATS_NET_EDEV_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, snedc->interface))
				/* Device not found */
				continue;
		}

		j = check_net_edev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			snedp = &snedzero;
		}
		else {
			snedp = (struct stats_net_edev *) ((char *) a->buf[!curr] + j * a->msize);
		}

		render(isdb, pre, PT_NOFLAG,
		       "%s\trxerr/s", "%s",
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->rx_errors, snedc->rx_errors, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\ttxerr/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->tx_errors, snedc->tx_errors, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\tcoll/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->collisions, snedc->collisions, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\trxdrop/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->rx_dropped, snedc->rx_dropped, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\ttxdrop/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->tx_dropped, snedc->tx_dropped, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\ttxcarr/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->tx_carrier_errors, snedc->tx_carrier_errors, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\trxfram/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->rx_frame_errors, snedc->rx_frame_errors, itv),
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\trxfifo/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->rx_fifo_errors, snedc->rx_fifo_errors, itv),
		       NULL);

		render(isdb, pre, pt_newlin,
		       "%s\ttxfifo/s", NULL,
		       cons(sv, snedc->interface, NULL),
		       NOVAL,
		       S_VALUE(snedp->tx_fifo_errors, snedc->tx_fifo_errors, itv),
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display NFS client statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_nfs_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr],
		*snnp = (struct stats_net_nfs *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tcall/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snnp->nfs_rpccnt, snnc->nfs_rpccnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tretrans/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snnp->nfs_rpcretrans, snnc->nfs_rpcretrans, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tread/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snnp->nfs_readcnt, snnc->nfs_readcnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\twrite/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snnp->nfs_writecnt, snnc->nfs_writecnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\taccess/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snnp->nfs_accesscnt, snnc->nfs_accesscnt, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tgetatt/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snnp->nfs_getattcnt, snnc->nfs_getattcnt, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display NFS server statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_nfsd_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr],
		*snndp = (struct stats_net_nfsd *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tscall/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_rpccnt, snndc->nfsd_rpccnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tbadcall/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_rpcbad, snndc->nfsd_rpcbad, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpacket/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_netcnt, snndc->nfsd_netcnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tudp/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_netudpcnt, snndc->nfsd_netudpcnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\ttcp/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_nettcpcnt, snndc->nfsd_nettcpcnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\thit/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_rchits, snndc->nfsd_rchits, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tmiss/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_rcmisses, snndc->nfsd_rcmisses, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tsread/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_readcnt, snndc->nfsd_readcnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tswrite/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_writecnt, snndc->nfsd_writecnt, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tsaccess/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_accesscnt, snndc->nfsd_accesscnt, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tsgetatt/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snndp->nfsd_getattcnt, snndc->nfsd_getattcnt, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display network sockets statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_sock_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_USEINT,
	       "-\ttotsck", NULL, NULL,
	       (unsigned long long) snsc->sock_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\ttcpsck", NULL, NULL,
	       (unsigned long long) snsc->tcp_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tudpsck",  NULL, NULL,
	       (unsigned long long) snsc->udp_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\trawsck", NULL, NULL,
	       (unsigned long long) snsc->raw_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tip-frag", NULL, NULL,
	       (unsigned long long) snsc->frag_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT | pt_newlin,
	       "-\ttcp-tw", NULL, NULL,
	       (unsigned long long) snsc->tcp_tw, DNOVAL, NULL);
}

/*
 ***************************************************************************
 * Display IP network statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_ip_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr],
		*snip = (struct stats_net_ip *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tirec/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InReceives, snic->InReceives, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tfwddgm/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->ForwDatagrams, snic->ForwDatagrams, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidel/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InDelivers, snic->InDelivers, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\torq/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutRequests, snic->OutRequests, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tasmrq/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->ReasmReqds, snic->ReasmReqds, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tasmok/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->ReasmOKs, snic->ReasmOKs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tfragok/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->FragOKs, snic->FragOKs, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tfragcrt/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->FragCreates, snic->FragCreates, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display IP network errors statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_eip_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr],
		*sneip = (struct stats_net_eip *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tihdrerr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InHdrErrors, sneic->InHdrErrors, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiadrerr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InAddrErrors, sneic->InAddrErrors, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiukwnpr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InUnknownProtos, sneic->InUnknownProtos, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidisc/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InDiscards, sneic->InDiscards, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\todisc/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutDiscards, sneic->OutDiscards, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tonort/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutNoRoutes, sneic->OutNoRoutes, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tasmf/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->ReasmFails, sneic->ReasmFails, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tfragf/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->FragFails, sneic->FragFails, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display ICMP network statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_icmp_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr],
		*snip = (struct stats_net_icmp *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\timsg/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InMsgs, snic->InMsgs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tomsg/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutMsgs, snic->OutMsgs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiech/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InEchos, snic->InEchos, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiechr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InEchoReps, snic->InEchoReps, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toech/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutEchos, snic->OutEchos, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toechr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutEchoReps, snic->OutEchoReps, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\titm/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InTimestamps, snic->InTimestamps, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\titmr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InTimestampReps, snic->InTimestampReps, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\totm/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutTimestamps, snic->OutTimestamps, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\totmr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutTimestampReps, snic->OutTimestampReps, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiadrmk/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InAddrMasks, snic->InAddrMasks, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiadrmkr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InAddrMaskReps, snic->InAddrMaskReps, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toadrmk/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutAddrMasks, snic->OutAddrMasks, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\toadrmkr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutAddrMaskReps, snic->OutAddrMaskReps, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display ICMP error messages statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_eicmp_stats(struct activity *a, int isdb, char *pre,
				       int curr, unsigned long long itv)
{
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr],
		*sneip = (struct stats_net_eicmp *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tierr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InErrors, sneic->InErrors, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toerr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutErrors, sneic->OutErrors, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidstunr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InDestUnreachs, sneic->InDestUnreachs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\todstunr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutDestUnreachs, sneic->OutDestUnreachs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\titmex/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InTimeExcds, sneic->InTimeExcds, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\totmex/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutTimeExcds, sneic->OutTimeExcds, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiparmpb/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InParmProbs, sneic->InParmProbs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toparmpb/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutParmProbs, sneic->OutParmProbs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tisrcq/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InSrcQuenchs, sneic->InSrcQuenchs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tosrcq/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutSrcQuenchs, sneic->OutSrcQuenchs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiredir/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InRedirects, sneic->InRedirects, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\toredir/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutRedirects, sneic->OutRedirects, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display TCP network statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_tcp_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr],
		*sntp = (struct stats_net_tcp *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tactive/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sntp->ActiveOpens, sntc->ActiveOpens, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tpassive/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sntp->PassiveOpens, sntc->PassiveOpens, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiseg/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sntp->InSegs, sntc->InSegs, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\toseg/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sntp->OutSegs, sntc->OutSegs, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display TCP network errors statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_etcp_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr],
		*snetp = (struct stats_net_etcp *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tatmptf/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snetp->AttemptFails, snetc->AttemptFails, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\testres/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snetp->EstabResets, snetc->EstabResets, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tretrans/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snetp->RetransSegs, snetc->RetransSegs, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tisegerr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snetp->InErrs, snetc->InErrs, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\torsts/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snetp->OutRsts, snetc->OutRsts, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display UDP network statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_udp_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr],
		*snup = (struct stats_net_udp *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidgm/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->InDatagrams, snuc->InDatagrams, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\todgm/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->OutDatagrams, snuc->OutDatagrams, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tnoport/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->NoPorts, snuc->NoPorts, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tidgmerr/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->InErrors, snuc->InErrors, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display IPv6 network sockets statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_sock6_stats(struct activity *a, int isdb, char *pre,
				       int curr, unsigned long long itv)
{
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_USEINT,
	       "-\ttcp6sck", NULL, NULL,
	       (unsigned long long) snsc->tcp6_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tudp6sck",  NULL, NULL,
	       (unsigned long long) snsc->udp6_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\traw6sck", NULL, NULL,
	       (unsigned long long) snsc->raw6_inuse, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT | pt_newlin,
	       "-\tip6-frag", NULL, NULL,
	       (unsigned long long) snsc->frag6_inuse, DNOVAL, NULL);
}

/*
 ***************************************************************************
 * Display IPv6 network statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_ip6_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr],
		*snip = (struct stats_net_ip6 *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tirec6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InReceives6, snic->InReceives6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tfwddgm6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutForwDatagrams6, snic->OutForwDatagrams6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidel6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InDelivers6, snic->InDelivers6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\torq6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutRequests6, snic->OutRequests6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tasmrq6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->ReasmReqds6, snic->ReasmReqds6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tasmok6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->ReasmOKs6, snic->ReasmOKs6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\timcpck6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InMcastPkts6, snic->InMcastPkts6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tomcpck6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutMcastPkts6, snic->OutMcastPkts6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tfragok6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->FragOKs6, snic->FragOKs6, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tfragcr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->FragCreates6, snic->FragCreates6, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display IPv6 network errors statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_eip6_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr],
		*sneip = (struct stats_net_eip6 *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tihdrer6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InHdrErrors6, sneic->InHdrErrors6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiadrer6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InAddrErrors6, sneic->InAddrErrors6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiukwnp6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InUnknownProtos6, sneic->InUnknownProtos6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\ti2big6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InTooBigErrors6, sneic->InTooBigErrors6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidisc6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InDiscards6, sneic->InDiscards6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\todisc6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutDiscards6, sneic->OutDiscards6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tinort6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InNoRoutes6, sneic->InNoRoutes6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tonort6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutNoRoutes6, sneic->OutNoRoutes6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tasmf6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->ReasmFails6, sneic->ReasmFails6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tfragf6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->FragFails6, sneic->FragFails6, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\titrpck6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InTruncatedPkts6, sneic->InTruncatedPkts6, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display ICMPv6 network statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_icmp6_stats(struct activity *a, int isdb, char *pre,
				       int curr, unsigned long long itv)
{
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr],
		*snip = (struct stats_net_icmp6 *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\timsg6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InMsgs6, snic->InMsgs6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tomsg6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutMsgs6, snic->OutMsgs6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiech6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InEchos6, snic->InEchos6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiechr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InEchoReplies6, snic->InEchoReplies6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toechr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutEchoReplies6, snic->OutEchoReplies6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tigmbq6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InGroupMembQueries6, snic->InGroupMembQueries6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tigmbr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InGroupMembResponses6, snic->InGroupMembResponses6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\togmbr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutGroupMembResponses6, snic->OutGroupMembResponses6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tigmbrd6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InGroupMembReductions6, snic->InGroupMembReductions6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\togmbrd6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutGroupMembReductions6, snic->OutGroupMembReductions6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tirtsol6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InRouterSolicits6, snic->InRouterSolicits6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tortsol6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutRouterSolicits6, snic->OutRouterSolicits6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tirtad6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InRouterAdvertisements6, snic->InRouterAdvertisements6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tinbsol6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InNeighborSolicits6, snic->InNeighborSolicits6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tonbsol6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutNeighborSolicits6, snic->OutNeighborSolicits6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tinbad6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->InNeighborAdvertisements6, snic->InNeighborAdvertisements6, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tonbad6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snip->OutNeighborAdvertisements6, snic->OutNeighborAdvertisements6, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display ICMPv6 error messages statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_eicmp6_stats(struct activity *a, int isdb, char *pre,
					int curr, unsigned long long itv)
{
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr],
		*sneip = (struct stats_net_eicmp6 *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tierr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InErrors6, sneic->InErrors6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidtunr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InDestUnreachs6, sneic->InDestUnreachs6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\todtunr6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutDestUnreachs6, sneic->OutDestUnreachs6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\titmex6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InTimeExcds6, sneic->InTimeExcds6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\totmex6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutTimeExcds6, sneic->OutTimeExcds6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiprmpb6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InParmProblems6, sneic->InParmProblems6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toprmpb6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutParmProblems6, sneic->OutParmProblems6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tiredir6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InRedirects6, sneic->InRedirects6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\toredir6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutRedirects6, sneic->OutRedirects6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tipck2b6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->InPktTooBigs6, sneic->InPktTooBigs6, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\topck2b6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(sneip->OutPktTooBigs6, sneic->OutPktTooBigs6, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display UDP6 network statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_net_udp6_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr],
		*snup = (struct stats_net_udp6 *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\tidgm6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->InDatagrams6, snuc->InDatagrams6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\todgm6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->OutDatagrams6, snuc->OutDatagrams6, itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\tnoport6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->NoPorts6, snuc->NoPorts6, itv),
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\tidgmer6/s", NULL, NULL,
	       NOVAL,
	       S_VALUE(snup->InErrors6, snuc->InErrors6, itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display CPU frequency statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_pwr_cpufreq_stats(struct activity *a, int isdb, char *pre,
					 int curr, unsigned long long itv)
{
	int i;
	struct stats_pwr_cpufreq *spc;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		if (!spc->cpufreq)
			/* This CPU is offline: Don't display it */
			continue;

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		if (!i) {
			/* This is CPU "all" */
			render(isdb, pre, pt_newlin,
			       "all\tMHz",
			       "-1", NULL,
			       NOVAL,
			       ((double) spc->cpufreq) / 100,
			       NULL);
		}
		else {
			render(isdb, pre, pt_newlin,
			       "cpu%d\tMHz",
			       "%d", cons(iv, i - 1, NOVAL),
			       NOVAL,
			       ((double) spc->cpufreq) / 100,
			       NULL);
		}
	}
}

/*
 ***************************************************************************
 * Display fan statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_pwr_fan_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	int i;
	struct stats_pwr_fan *spc;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + i * a->msize);

		render(isdb, pre, PT_USESTR,
		       "fan%d\tDEVICE",
		       "%d",
		       cons(iv, i + 1, NOVAL),
		       NOVAL,
		       NOVAL,
		       spc->device);

		render(isdb, pre, PT_NOFLAG,
		       "fan%d\trpm",
		       NULL,
		       cons(iv, i + 1, NOVAL),
		       NOVAL,
		       spc->rpm,
		       NULL);

		render(isdb, pre, pt_newlin,
		       "fan%d\tdrpm",
		       NULL,
		       cons(iv, i + 1, NOVAL),
		       NOVAL,
		       spc->rpm - spc->rpm_min,
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display temperature statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_pwr_temp_stats(struct activity *a, int isdb, char *pre,
				      int curr, unsigned long long itv)
{
	int i;
	struct stats_pwr_temp *spc;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);

		render(isdb, pre, PT_USESTR,
		       "temp%d\tDEVICE",
		       "%d",
		       cons(iv, i + 1, NOVAL),
		       NOVAL,
		       NOVAL,
		       spc->device);

		render(isdb, pre, PT_NOFLAG,
		       "temp%d\tdegC",
		       NULL,
		       cons(iv, i + 1, NOVAL),
		       NOVAL,
		       spc->temp,
		       NULL);

		render(isdb, pre, pt_newlin,
		       "temp%d\t%%temp",
		       NULL,
		       cons(iv, i + 1, NOVAL),
		       NOVAL,
		       (spc->temp_max - spc->temp_min) ?
		       (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100 :
		       0.0,
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display voltage inputs statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_pwr_in_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	int i;
	struct stats_pwr_in *spc;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + i * a->msize);

		render(isdb, pre, PT_USESTR,
		       "in%d\tDEVICE",
		       "%d",
		       cons(iv, i, NOVAL),
		       NOVAL,
		       NOVAL,
		       spc->device);

		render(isdb, pre, PT_NOFLAG,
		       "in%d\tinV",
		       NULL,
		       cons(iv, i, NOVAL),
		       NOVAL,
		       spc->in,
		       NULL);

		render(isdb, pre, pt_newlin,
		       "in%d\t%%in",
		       NULL,
		       cons(iv, i, NOVAL),
		       NOVAL,
		       (spc->in_max - spc->in_min) ?
		       (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100 :
		       0.0,
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display huge pages statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_huge_stats(struct activity *a, int isdb, char *pre,
				  int curr, unsigned long long itv)
{
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_USEINT,
	       "-\tkbhugfree", NULL, NULL,
	       smc->frhkb, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tkbhugused", NULL, NULL,
	       smc->tlhkb - smc->frhkb, DNOVAL, NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%hugused", NULL, NULL, NOVAL,
	       smc->tlhkb ?
	       SP_VALUE(smc->frhkb, smc->tlhkb, smc->tlhkb) :
	       0.0, NULL);

	render(isdb, pre, PT_USEINT,
	       "-\tkbhugrsvd", NULL, NULL,
	       smc->rsvdhkb, DNOVAL, NULL);

	render(isdb, pre, PT_USEINT | pt_newlin,
	       "-\tkbhugsurp", NULL, NULL,
	       smc->surphkb, DNOVAL, NULL);
}

/*
 ***************************************************************************
 * Display weighted CPU frequency statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_pwr_wghfreq_stats(struct activity *a, int isdb, char *pre,
					 int curr, unsigned long long itv)
{
	int i, k;
	struct stats_pwr_wghfreq *spc, *spp, *spc_k, *spp_k;
	unsigned long long tis, tisfreq;
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_wghfreq *) ((char *) a->buf[curr]  + i * a->msize * a->nr2);
		spp = (struct stats_pwr_wghfreq *) ((char *) a->buf[!curr] + i * a->msize * a->nr2);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		tisfreq = 0;
		tis = 0;

		for (k = 0; k < a->nr2; k++) {

			spc_k = (struct stats_pwr_wghfreq *) ((char *) spc + k * a->msize);
			if (!spc_k->freq)
				break;
			spp_k = (struct stats_pwr_wghfreq *) ((char *) spp + k * a->msize);

			tisfreq += (spc_k->freq / 1000) *
			           (spc_k->time_in_state - spp_k->time_in_state);
			tis     += (spc_k->time_in_state - spp_k->time_in_state);
		}

		if (!i) {
			/* This is CPU "all" */
			render(isdb, pre, pt_newlin,
			       "all\twghMHz",
			       "-1", NULL,
			       NOVAL,
			       tis ? ((double) tisfreq) / tis : 0.0,
			       NULL);
		}
		else {
			render(isdb, pre, pt_newlin,
			       "cpu%d\twghMHz",
			       "%d", cons(iv, i - 1, NOVAL),
			       NOVAL,
			       tis ? ((double) tisfreq) / tis : 0.0,
			       NULL);
		}
	}
}

/*
 ***************************************************************************
 * Display USB devices statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_pwr_usb_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	int i;
	struct stats_pwr_usb *suc;
	char id[9];

	for (i = 0; i < a->nr[curr]; i++) {
		suc = (struct stats_pwr_usb *) ((char *) a->buf[curr] + i * a->msize);

		sprintf(id, "%x", suc->vendor_id);
		render(isdb, pre, PT_USESTR,
		       "bus%d\tidvendor",
		       "%d",
		       cons(iv, suc->bus_nr, NOVAL),
		       NOVAL,
		       NOVAL,
		       id);

		sprintf(id, "%x", suc->product_id);
		render(isdb, pre, PT_USESTR,
		       "bus%d\tidprod",
		       NULL,
		       cons(iv, suc->bus_nr, NOVAL),
		       NOVAL,
		       NOVAL,
		       id);

		render(isdb, pre, PT_USEINT,
		       "bus%d\tmaxpower",
		       NULL,
		       cons(iv, suc->bus_nr, NOVAL),
		       (unsigned long long) (suc->bmaxpower << 1),
		       NOVAL,
		       NULL);

		render(isdb, pre, PT_USESTR,
		       "bus%d\tmanufact",
		       NULL,
		       cons(iv, suc->bus_nr, NOVAL),
		       NOVAL,
		       NOVAL,
		       suc->manufacturer);

		render(isdb, pre,
		       (DISPLAY_HORIZONTALLY(flags) ? PT_USESTR : PT_USESTR | PT_NEWLIN),
		       "bus%d\tproduct",
		       NULL,
		       cons(iv, suc->bus_nr, NOVAL),
		       NOVAL,
		       NOVAL,
		       suc->product);
	}
}

/*
 ***************************************************************************
 * Display filesystems statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_filesystem_stats(struct activity *a, int isdb, char *pre,
					int curr, unsigned long long itv)
{
	int i;
	struct stats_filesystem *sfc;

	for (i = 0; i < a->nr[curr]; i++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list,
					      DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name))
				/* Device not found */
				continue;
		}

		render(isdb, pre, PT_USERND,
		       "%s\tMBfsfree",
		       "%s",
		       cons(sv, DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, NOVAL),
		       NOVAL,
		       (double) sfc->f_bfree / 1024 / 1024,
		       NULL);

		render(isdb, pre, PT_USERND,
		       "%s\tMBfsused",
		       NULL,
		       cons(sv, DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, NOVAL),
		       NOVAL,
		       (double) (sfc->f_blocks - sfc->f_bfree) / 1024 / 1024,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\t%%fsused",
		       NULL,
		       cons(sv, DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, NOVAL),
		       NOVAL,
		       sfc->f_blocks ? SP_VALUE(sfc->f_bfree, sfc->f_blocks, sfc->f_blocks)
				     : 0.0,
		       NULL);

		render(isdb, pre, PT_NOFLAG,
		       "%s\t%%ufsused",
		       NULL,
		       cons(sv, DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, NOVAL),
		       NOVAL,
		       sfc->f_blocks ? SP_VALUE(sfc->f_bavail, sfc->f_blocks, sfc->f_blocks)
				     : 0.0,
		       NULL);

		render(isdb, pre, PT_USEINT,
		       "%s\tIfree",
		       NULL,
		       cons(sv, DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, NOVAL),
		       sfc->f_ffree,
		       NOVAL,
		       NULL);

		render(isdb, pre, PT_USEINT,
		       "%s\tIused",
		       NULL,
		       cons(sv, DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, NOVAL),
		       sfc->f_files - sfc->f_ffree,
		       NOVAL,
		       NULL);

		render(isdb, pre,
		       (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN),
		       "%s\t%%Iused",
		       NULL,
		       cons(sv, DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, NOVAL),
		       NOVAL,
		       sfc->f_files ? SP_VALUE(sfc->f_ffree, sfc->f_files, sfc->f_files)
				    : 0.0,
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display Fibre Channel HBA statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_fchost_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	int i, j, j0, found;
	struct stats_fchost *sfcc, *sfcp, sfczero;

	memset(&sfczero, 0, sizeof(struct stats_fchost));

	for (i = 0; i < a->nr[curr]; i++) {

		found = FALSE;
		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + i * a->msize);

		if (a->nr[!curr] > 0) {
			/* Look for corresponding structure in previous iteration */
			j = i;

			if (j >= a->nr[!curr]) {
				j = a->nr[!curr] - 1;
			}

			j0 = j;

			do {
				sfcp = (struct stats_fchost *) ((char *) a->buf[!curr] + j * a->msize);
				if (!strcmp(sfcc->fchost_name, sfcp->fchost_name)) {
					found = TRUE;
					break;
				}
				if (++j >= a->nr[!curr]) {
					j = 0;
				}
			}
			while (j != j0);
		}

		if (!found) {
			/* This is a newly registered host */
			sfcp = &sfczero;
		}

		render(isdb, pre, PT_NOFLAG ,
		       "%s\tfch_rxf/s",
		       "%s",
		       cons(sv, sfcc->fchost_name, NOVAL),
		       NOVAL,
		       S_VALUE(sfcp->f_rxframes, sfcc->f_rxframes, itv),
	               NULL);
		render(isdb, pre, PT_NOFLAG,
		       "%s\tfch_txf/s", NULL,
		       cons(sv, sfcc->fchost_name, NULL),
		       NOVAL,
		       S_VALUE(sfcp->f_txframes, sfcc->f_txframes, itv),
		       NULL);
		render(isdb, pre, PT_NOFLAG,
		       "%s\tfch_rxw/s", NULL,
		       cons(sv, sfcc->fchost_name, NULL),
		       NOVAL,
		       S_VALUE(sfcp->f_rxwords, sfcc->f_rxwords, itv),
		       NULL);
		render(isdb, pre,
		       (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN),
		       "%s\tfch_txw/s", NULL,
		       cons(sv, sfcc->fchost_name, NULL),
		       NOVAL,
		       S_VALUE(sfcp->f_txwords, sfcc->f_txwords, itv),
		       NULL);
	}
}

/*
 ***************************************************************************
 * Display softnet statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_softnet_stats(struct activity *a, int isdb, char *pre,
				     int curr, unsigned long long itv)
{
	int i;
	struct stats_softnet *ssnc, *ssnp;
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	/* @nr[curr] cannot normally be greater than @nr_ini */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Compute statistics for CPU "all" */
	get_global_soft_statistics(a, !curr, curr, flags, offline_cpu_bitmap);

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * Note: a->nr is in [1, NR_CPUS + 1].
		 * Bitmap size is provided for (NR_CPUS + 1) CPUs.
		 * Anyway, NR_CPUS may vary between the version of sysstat
		 * used by sadc to create a file, and the version of sysstat
		 * used by sar to read it...
		 */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* No */
			continue;

		/*
		 * The size of a->buf[...] CPU structure may be different from the default
		 * sizeof(struct stats_pwr_cpufreq) value if data have been read from a file!
		 * That's why we don't use a syntax like:
		 * ssnc = (struct stats_softnet *) a->buf[...] + i;
                 */
                ssnc = (struct stats_softnet *) ((char *) a->buf[curr]  + i * a->msize);
                ssnp = (struct stats_softnet *) ((char *) a->buf[!curr] + i * a->msize);

		if (!i) {
			/* This is CPU "all" */
			render(isdb, pre, PT_NOFLAG,
			       "all\ttotal/s",
			       "-1", NULL,
			       NOVAL,
			       S_VALUE(ssnp->processed, ssnc->processed, itv),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "all\tdropd/s",
			       NULL, NULL,
			       NOVAL,
			       S_VALUE(ssnp->dropped, ssnc->dropped, itv),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "all\tsqueezd/s",
			       NULL, NULL,
			       NOVAL,
			       S_VALUE(ssnp->time_squeeze, ssnc->time_squeeze, itv),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "all\trx_rps/s",
			       NULL, NULL,
			       NOVAL,
			       S_VALUE(ssnp->received_rps, ssnc->received_rps, itv),
			       NULL);

			render(isdb, pre, pt_newlin,
			       "all\tflw_lim/s",
			       NULL, NULL,
			       NOVAL,
			       S_VALUE(ssnp->flow_limit, ssnc->flow_limit, itv),
			       NULL);
		}
		else {
			render(isdb, pre, PT_NOFLAG,
			       "cpu%d\ttotal/s",
			       "%d", cons(iv, i - 1, NOVAL),
			       NOVAL,
			       S_VALUE(ssnp->processed, ssnc->processed, itv),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "cpu%d\tdropd/s",
			       NULL, cons(iv, i - 1, NOVAL),
			       NOVAL,
			       S_VALUE(ssnp->dropped, ssnc->dropped, itv),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "cpu%d\tsqueezd/s",
			       NULL, cons(iv, i - 1, NOVAL),
			       NOVAL,
			       S_VALUE(ssnp->time_squeeze, ssnc->time_squeeze, itv),
			       NULL);

			render(isdb, pre, PT_NOFLAG,
			       "cpu%d\trx_rps/s",
			       NULL, cons(iv, i - 1, NOVAL),
			       NOVAL,
			       S_VALUE(ssnp->received_rps, ssnc->received_rps, itv),
			       NULL);

			render(isdb, pre, pt_newlin,
			       "cpu%d\tflw_lim/s",
			       NULL, cons(iv, i - 1, NOVAL),
			       NOVAL,
			       S_VALUE(ssnp->flow_limit, ssnc->flow_limit, itv),
			       NULL);
		}
	}
}

/*
 ***************************************************************************
 * Display pressure-stall CPU statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_psicpu_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr],
		*psip = (struct stats_psi_cpu *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%scpu-10", NULL, NULL,
	       NOVAL,
	       (double) psic->some_acpu_10 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%scpu-60", NULL, NULL,
	       NOVAL,
	       (double) psic->some_acpu_60 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%scpu-300", NULL, NULL,
	       NOVAL,
	       (double) psic->some_acpu_300 / 100,
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\t%scpu", NULL, NULL,
	       NOVAL,
	       ((double) psic->some_cpu_total - psip->some_cpu_total) / (100 * itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display pressure-stall I/O statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_psiio_stats(struct activity *a, int isdb, char *pre,
				   int curr, unsigned long long itv)
{
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr],
		*psip = (struct stats_psi_io *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%sio-10", NULL, NULL,
	       NOVAL,
	       (double) psic->some_aio_10 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%sio-60", NULL, NULL,
	       NOVAL,
	       (double) psic->some_aio_60 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%sio-300", NULL, NULL,
	       NOVAL,
	       (double) psic->some_aio_300 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%sio", NULL, NULL,
	       NOVAL,
	       ((double) psic->some_io_total - psip->some_io_total) / (100 * itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%fio-10", NULL, NULL,
	       NOVAL,
	       (double) psic->full_aio_10 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%fio-60", NULL, NULL,
	       NOVAL,
	       (double) psic->full_aio_60 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%fio-300", NULL, NULL,
	       NOVAL,
	       (double) psic->full_aio_300 / 100,
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\t%fio", NULL, NULL,
	       NOVAL,
	       ((double) psic->full_io_total - psip->full_io_total) / (100 * itv),
	       NULL);
}

/*
 ***************************************************************************
 * Display pressure-stall memory statistics in selected format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @isdb	Flag, true if db printing, false if ppc printing.
 * @pre		Prefix string for output entries
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t render_psimem_stats(struct activity *a, int isdb, char *pre,
				    int curr, unsigned long long itv)
{
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr],
		*psip = (struct stats_psi_mem *) a->buf[!curr];
	int pt_newlin
		= (DISPLAY_HORIZONTALLY(flags) ? PT_NOFLAG : PT_NEWLIN);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%smem-10", NULL, NULL,
	       NOVAL,
	       (double) psic->some_amem_10 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%smem-60", NULL, NULL,
	       NOVAL,
	       (double) psic->some_amem_60 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%smem-300", NULL, NULL,
	       NOVAL,
	       (double) psic->some_amem_300 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%smem", NULL, NULL,
	       NOVAL,
	       ((double) psic->some_mem_total - psip->some_mem_total) / (100 * itv),
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%fmem-10", NULL, NULL,
	       NOVAL,
	       (double) psic->full_amem_10 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%fmem-60", NULL, NULL,
	       NOVAL,
	       (double) psic->full_amem_60 / 100,
	       NULL);

	render(isdb, pre, PT_NOFLAG,
	       "-\t%fmem-300", NULL, NULL,
	       NOVAL,
	       (double) psic->full_amem_300 / 100,
	       NULL);

	render(isdb, pre, pt_newlin,
	       "-\t%fmem", NULL, NULL,
	       NOVAL,
	       ((double) psic->full_mem_total - psip->full_mem_total) / (100 * itv),
	       NULL);
}
