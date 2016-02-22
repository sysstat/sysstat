/*
 * svg_stats.c: Funtions used by sadf to display statistics in SVG format.
 * (C) 2016 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <stdlib.h>
#include <float.h>

#include "sa.h"
#include "sadf.h"
#include "ioconf.h"
#include "svg_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern unsigned int flags;
extern unsigned int dm_major;

unsigned int svg_colors[] = {0x00cc00, 0xff00bf, 0x00ffff, 0xff0000,
			     0x0000ff, 0xffbf00, 0x00ff00, 0x7030a0,
			     0xffffbf, 0xffff00, 0xd60093, 0x00bfbf,
			     0xcc3300, 0xbfbfbf, 0x666635, 0xff3300};
#define SVG_COLORS_IDX_MASK	0x0f

/*
 ***************************************************************************
 * Compare the values of a statistics sample with the max and min values
 * already found in previous samples for this same activity. If some new
 * min or max values are found, then save them.
 * The structure containing the statistics sample is composed of @llu_nr
 * unsigned long long fields, followed by @lu_nr unsigned long fields, then
 * followed by @u_nr unsigned int fields.
 *
 * IN:
 * @llu_nr	Number of unsigned long long fields composing the structure.
 * @lu_nr	Number of unsigned long fields composing the structure.
 * @u_nr	Number of unsigned int fields composing the structure.
 * @a		Activity structure containing current statistics sample.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in jiffies.
 * @minv	Array containing min values already found for this activity.
 * @maxv	Array containing max values already found for this activity.
 *
 * OUT:
 * @minv	Array containg the possible new min values for current activity.
 * @maxv	Array containg the possible new max values for current activity.
 *
 * NB: @minv and @maxv arrays contain values in the same order as the fields
 * in the statistics structure.
 ***************************************************************************
 */
void save_extrema(int llu_nr, int lu_nr, int u_nr, struct activity *a, int curr,
		  unsigned long long itv, double minv[], double maxv[])
{
	unsigned long long *lluc, *llup;
	unsigned long *luc, *lup;
	unsigned int *uc, *up;
	double val;
	int i, m = 0;

	/* Compare unsigned long long fields */
	lluc = (unsigned long long *) a->buf[curr];
	llup = (unsigned long long *) a->buf[!curr];
	for (i = 0; i < llu_nr; i++, m++) {
		val = S_VALUE(*llup, *lluc, itv);
		if (val < minv[m]) {
			minv[m] = val;
		}
		if (val > maxv[m]) {
			maxv[m] = val;
		}
		lluc = (unsigned long long *) ((char *) lluc + ULL_ALIGNMENT_WIDTH);
		llup = (unsigned long long *) ((char *) llup + ULL_ALIGNMENT_WIDTH);
	}

	/* Compare unsigned long fields */
	luc = (unsigned long *) lluc;
	lup = (unsigned long *) llup;
	for (i = 0; i < lu_nr; i++, m++) {
		val = S_VALUE(*lup, *luc, itv);
		if (val < minv[m]) {
			minv[m] = val;
		}
		if (val > maxv[m]) {
			maxv[m] = val;
		}
		luc = (unsigned long *) ((char *) luc + UL_ALIGNMENT_WIDTH);
		lup = (unsigned long *) ((char *) lup + UL_ALIGNMENT_WIDTH);
	}

	/* Compare unsigned int fields */
	uc = (unsigned int *) luc;
	up = (unsigned int *) lup;
	for (i = 0; i < u_nr; i++, m++) {
		val = S_VALUE(*up, *uc, itv);
		if (val < minv[m]) {
			minv[m] = val;
		}
		if (val > maxv[m]) {
			maxv[m] = val;
		}
		uc = (unsigned int *) ((char *) uc + U_ALIGNMENT_WIDTH);
		up = (unsigned int *) ((char *) up + U_ALIGNMENT_WIDTH);
	}
}

/*
 ***************************************************************************
 * Find the min and max values of all the graphs that will be drawn in the
 * same window. The graphs have their own min and max values in
 * minv[pos...pos+n-1] and maxv[pos...pos+n-1]. 
 *
 * IN:
 * @pos		Position in array for the first graph extrema value.
 * @n		Number of graphs to scan.
 * @minv	Array containing min values for graphs.
 * @maxv	Array containing max values for graphs.
 *
 * OUT:
 * @minv	minv[pos] is modified and contains the global min value found.
 * @maxv	maxv[pos] is modified and contains the global max value found.
 ***************************************************************************
 */
void get_global_extrema(int pos, int n, double minv[], double maxv[])
{
	int i;

	for (i = 1; i < n; i++) {
		if (minv[pos + i] < minv[pos]) {
			minv[pos] = minv[pos + i];
		}
		if (maxv[pos + i] > maxv[pos]) {
			maxv[pos] = maxv[pos + i];
		}
	}
}

/*
 ***************************************************************************
 * Allocate arrays used to save graphs data, min and max values.
 * @n arrays of chars are allocated for @n graphs to draw. A pointer on this
 * array is returned. This is equivalent to "char data[][n]" where each
 * element is of indeterminate size and will contain the graph data (eg.
 * << path d="M12,14 L13,16..." ... >>.
 * The size of element data[i] is given by outsize[i].
 * Also allocate an array to save min values (equivalent to "double spmin[n]")
 * and an array for max values (equivalent to "double spmax[n]").
 *
 * IN:
 * @n		Number of graphs to draw for current activity.
 *
 * OUT:
 * @outsize	Array that will contain the sizes of each element in array
 *		of chars. Equivalent to "int outsize[n]" with
 * 		outsize[n] = sizeof(data[][n]).
 * @spmin	Array that will contain min values for current activity.
 * @spmax	Array that will contain max values for current activity.
 *
 * RETURNS:
 * Pointer on array of arrays of chars that will contain the graphs data.
 *
 * NB: @min and @max arrays contain values in the same order as the fields
 * in the statistics structure.
 ***************************************************************************
 */
char **allocate_graph_lines(int n, int **outsize, double **spmin, double **spmax)
{
	char **out;
	char *out_p;
	int i;

	/*
	 * Allocate an array of pointers. Each of these pointers will
	 * be an array of chars.
	 */
	if ((out = (char **) malloc(n * sizeof(char *))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate array that will contain the size of each array of chars */
	if ((*outsize = (int *) malloc(n * sizeof(int))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate array that will contain the min value of each graph */
	if ((*spmin = (double *) malloc(n * sizeof(double))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate array that will contain the max value of each graph */
	if ((*spmax = (double *) malloc(n * sizeof(double))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate arrays of chars that will contain graphs data */
	for (i = 0; i < n; i++) {
		if ((out_p = (char *) malloc(CHUNKSIZE * sizeof(char))) == NULL) {
			perror("malloc");
			exit(4);
		}
		*(out + i) = out_p;
		*out_p = '\0';			/* Reset string so that it can be safely strncat()'d later */
		*(*outsize + i) = CHUNKSIZE;	/* Each array of chars has a default size of CHUNKSIZE */
		*(*spmin + i) = DBL_MAX;	/* Init min and max values */
		*(*spmax + i) = -DBL_MAX;
	}

	return out;
}

/*
 ***************************************************************************
 * Update graph definition by appending current X,Y coordinates.
 *
 * IN:
 * @timetag	Timestamp in seconds since the epoch for current sample
 *		stats. Will be used as X coordinate.
 * @value	Value of current sample metric. Will be used as Y coordinate.
 * @out		Pointer on array of chars for current graph definition.
 * @outsize	Size of array of chars for current graph definition.
 * @restart	Set to TRUE if a RESTART record has been read since the last
 * 		statistics sample.
 *
 * OUT:
 * @out		Pointer on array of chars for current graph definition that
 *		has been updated with the addition of current sample data.
 * @outsize	Array that containing the (possibly new) sizes of each
 *		element in array of chars.
 ***************************************************************************
 */
void lnappend(unsigned long timetag, double value, char **out, int *outsize, int restart)
{
	char point[128];
	char *out_p;
	int len;

	/* Prepare additional graph definition data */
	snprintf(point, 128, " %c%lu,%.2f", restart ? 'M' : 'L', timetag, value);
	point[127] = '\0';
	out_p = *out;
	len = *outsize - strlen(out_p) - 1;
	if (strlen(point) >= len) {
		/*
		 * If current array of chars doesn't have enough space left
		 * then reallocate it with CHUNKSIZE more bytes.
		 */
		SREALLOC(out_p, char, *outsize + CHUNKSIZE);
		*out = out_p;
		*outsize += CHUNKSIZE;
	}
	strncat(out_p, point, len);
}

/*
 ***************************************************************************
 * Calculate the value on the Y axis between two horizontal lines that will
 * make the graph background grid.
 *
 * IN:
 * @lmax	Max value reached for this graph.
 *
 * OUT:
 * @dp		Number of decimal places for Y graduations.
 *
 * RETURNS:
 * Value between two horizontal lines.
 ***************************************************************************
 */
double ygrid(double lmax, int *dp)
{
	char val[32];
	int l, i, e = 1;
	long n = 0;

	*dp = 0;
	if (lmax == 0) {
		lmax = 1;
	}
	n = (long) (lmax / SVG_H_GRIDNR);
	if (!n) {
		*dp = 2;
		return (lmax / SVG_H_GRIDNR);
	}
	snprintf(val, 32, "%ld", n);
	val[31] = '\0';
	l = strlen(val);
	if (l < 2)
		return n;
	for (i = 1; i < l; i++) {
		e = e * 10;
	}
	return ((double) (((long) (n / e)) * e));
}

/*
 ***************************************************************************
 * Calculate the value on the X axis between two vertical lines that will
 * make the graph background grid.
 *
 * IN:
 * @timestart	First data timestamp (X coordinate of the first data point).
 * @timeend	Last data timestamp (X coordinate of the last data point).
 *
 * RETURNS:
 * Value between two vertical lines.
 ***************************************************************************
 */
long int xgrid(unsigned long timestart, unsigned long timeend)
{
	return ((timeend - timestart) / SVG_V_GRIDNR);
}

/*
 ***************************************************************************
 * Display all graphs for current activity.
 *
 * IN:
 * @a		Activity structure containing the number of graphs to draw
 *		for current activity.
 * @title	Titles for each set of graphs.
 * @g_title	Titles for each graph.
 * @group	Indicate how graphs are grouped together to make sets.
 * @spmin	Array containing min values for graphs.
 * @spmax	Array containing max values for graphs.
 * @out		Pointer on array of chars for each graph definition.
 * @outsize	Size of array of chars for each graph definition.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no)
 *		and a pointer on a record header structure (.@record_hdr)
 *		containing the first stats sample.
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
void draw_activity_graphs(struct activity *a, char *title[], char *g_title[], int group[],
			  double *spmin, double *spmax, char **out, int *outsize,
			  struct svg_parm *svg_p, struct record_header *record_hdr)
{
	struct record_header stamp;
	struct tm rectime;
	char *out_p;
	int i, j, dp, pos = 0;
	long int k;
	double lmax, xfactor, yfactor, ypos;
	char cur_time[32];

	/* Translate to proper position for current activity */
	printf("<g id=\"g%d\" transform=\"translate(0,%d)\">\n",
	       svg_p->graph_no,
	       SVG_H_YSIZE + svg_p->graph_no * SVG_T_YSIZE);

	/* For each graphs set which are part of current activity */
	for (i = 0; i < a->g_nr; i++) {

		/* Graph background */
		printf("<rect x=\"0\" y=\"%d\" height=\"%d\" width=\"%d\"/>\n",
		       i * SVG_T_YSIZE,
		       SVG_V_YSIZE, SVG_V_XSIZE);

		/* Graph title */
		printf("<text x=\"0\" y=\"%d\" style=\"fill: yellow; stroke: none\">%s\n",
		       20 + i * SVG_T_YSIZE, title[i]);
		printf("<tspan x=\"%d\" y=\"%d\" style=\"fill: yellow; stroke: none; font-size: 12px\">"
		       "(Min, Max values)</tspan>\n</text>\n",
		       5 + SVG_M_XSIZE + SVG_G_XSIZE,
		       25 + i * SVG_T_YSIZE);

		/*
		 * At least two samples are needed.
		 * And a min and max value should have been found.
		 */
		if ((record_hdr->ust_time == svg_p->record_hdr->ust_time) ||
		    (*(spmin + i) == DBL_MAX) || (*(spmax + i) == -DBL_MIN)) {
			/* No data found */
			printf("<text x=\"0\" y=\"%d\" style=\"fill: red; stroke: none\">No data</text>\n",
			       SVG_M_YSIZE + i * SVG_T_YSIZE);
			continue;
		}

		/* X and Y axis */
		printf("<polyline points=\"%d,%d %d,%d %d,%d\" stroke=\"white\" stroke-width=\"2\"/>\n",
		       SVG_M_XSIZE, SVG_M_YSIZE + i * SVG_T_YSIZE,
		       SVG_M_XSIZE, SVG_M_YSIZE + SVG_G_YSIZE + i * SVG_T_YSIZE,
		       SVG_M_XSIZE + SVG_G_XSIZE, SVG_M_YSIZE + SVG_G_YSIZE + i * SVG_T_YSIZE);

		/* Caption */
		for (j = 0; j < group[i]; j++) {
			printf("<text x=\"%d\" y=\"%d\" style=\"fill: #%06x; stroke: none; font-size: 12px\">"
			       "%s (%.2f, %.2f)</text>\n",
			       5 + SVG_M_XSIZE + SVG_G_XSIZE, SVG_M_YSIZE + i * SVG_T_YSIZE + j * 15,
			       svg_colors[(pos + j) & SVG_COLORS_IDX_MASK], g_title[pos + j],
		       *(spmin + pos + j), *(spmax + pos + j));
		}

		/* Get global min and max value for current graphs set */
		get_global_extrema(pos, group[i], spmin, spmax);

		/* Translate to proper position for current graph within current activity */
		printf("<g transform=\"translate(%d,%d)\">\n",
		       SVG_M_XSIZE, SVG_M_YSIZE + SVG_G_YSIZE + i * SVG_T_YSIZE);

		/* Grid */
		if (*(spmax + pos) == 0) {
			/* If all values are zero then set current max value to 1 */
			lmax = 1.0;
		}
		else {
			lmax = *(spmax + pos);
		}
		ypos = ygrid(*(spmax + pos), &dp);
		yfactor = (double) -SVG_G_YSIZE / lmax;
		j = 1;
		do {
			printf("<polyline points=\"0,%.2f %d,%.2f\" vector-effect=\"non-scaling-stroke\" "
			       "stroke=\"#202020\" transform=\"scale(1,%f)\"/>\n",
			       ypos * j, SVG_G_XSIZE, ypos * j, yfactor);
			j++;
		}
		while (ypos * j <= lmax);
		j = 0;
		do {
			printf("<text x=\"0\" y=\"%ld\" style=\"fill: white; stroke: none; font-size: 12px; "
			       "text-anchor: end\">%.*f.</text>\n",
			       (long) (ypos * j * yfactor), dp, ypos * j);
			j++;
		}
		while (ypos * j <= lmax);

		k = xgrid(svg_p->record_hdr->ust_time, record_hdr->ust_time);
		xfactor = (double) SVG_G_XSIZE / (record_hdr->ust_time - svg_p->record_hdr->ust_time);
		stamp = *svg_p->record_hdr;
		for (j = 1; j <= SVG_V_GRIDNR; j++) {
			printf("<polyline points=\"%ld,0 %ld,%d\" vector-effect=\"non-scaling-stroke\" "
			       "stroke=\"#202020\" transform=\"scale(%f,1)\"/>\n",
			       k * j, k * j, -SVG_G_YSIZE, xfactor);
		}
		for (j = 0; j <= SVG_V_GRIDNR; j++) {
			sa_get_record_timestamp_struct(flags, &stamp, &rectime, NULL);
			set_record_timestamp_string(flags, &stamp, NULL, cur_time, 32, &rectime);
			printf("<text x=\"%ld\" y=\"10\" style=\"fill: white; stroke: none; font-size: 12px; "
			       "text-anchor: start\" transform=\"rotate(45,%ld,0)\">%s</text>\n",
			       (long) (k * j * xfactor), (long) (k * j * xfactor), cur_time);
			stamp.ust_time += k;
		}
		if (!PRINT_LOCAL_TIME(flags)) {
			printf("<text x=-10 y=\"30\" style=\"fill: yellow; stroke: none; font-size: 12px; "
			       "text-anchor: end\">UTC</text>\n");
		}

		/* Draw current graphs set */
		for (j = 0; j < group[i]; j++) {
			out_p = *(out + pos + j);
			printf("<path id=\"g%dp%d\" d=\"%s\" vector-effect=\"non-scaling-stroke\" "
			       "stroke=\"#%06x\" stroke-width=\"1\" fill-opacity=\"0\" "
			       "transform=\"scale(%f,%f)\"/>\n",
			       svg_p->graph_no, pos + j, out_p,
			       svg_colors[(pos + j) & SVG_COLORS_IDX_MASK],
			       xfactor,
			       yfactor);
			free(out_p);
		}
		printf("</g>\n");
		pos += group[i];
	}
	printf("</g>\n");

	/* Next graph */
	(svg_p->graph_no) += a->g_nr;

	/* Free remaining structures */
	free(out);
	free(outsize);
	free(spmin);
	free(spmax);
}

/*
 ***************************************************************************
 * Display task creation and context switch statistics in SVG
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and a pointer on a record header structure
 * 		(.@record_hdr) containing the first stats sample.
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_pcsw_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				     unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr],
		*spp = (struct stats_pcsw *) a->buf[!curr];
	int group[] = {1, 1};
	char *title[] = {"Switching activity", "Task creation"};
	char *g_title[] = {"cswch/s", "proc/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(2, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(1, 1, 0, a, curr, itv, spmin, spmax);
		/* cswch/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->context_switch, spc->context_switch, itv),
			 out, outsize, svg_p->restart);
		/* proc/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->processes, spc->processes, itv),
			 out + 1, outsize + 1, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a, title, g_title, group, spmin, spmax,
				     out, outsize, svg_p, record_hdr);
	}
}

/*
 ***************************************************************************
 * Display paging statistics in SVG
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and a pointer on a record header structure
 * 		(.@record_hdr) containing the first stats sample.
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_paging_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				       unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr],
		*spp = (struct stats_paging *) a->buf[!curr];
	int group[] = {2, 2, 4};
	char *title[] = {"Paging activity (1)", "Paging activity (2)", "Paging activity (3)"};
	char *g_title[] = {"pgpgin/s", "pgpgout/s", "fault/s", "majflt/s",
			   "pgfree/s", "pgscank/s", "pgscand/s", "pgsteal/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(8, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 8, 0, a, curr, itv, spmin, spmax);
		/* pgpgin/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgpgin, spc->pgpgin, itv),
			 out, outsize, svg_p->restart);
		/* pgpgout/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgpgout, spc->pgpgout, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* fault/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgfault, spc->pgfault, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* majflt/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgmajfault, spc->pgmajfault, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* pgfree/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgfree, spc->pgfree, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* pgscank/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgscan_kswapd, spc->pgscan_kswapd, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* pgscand/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgscan_direct, spc->pgscan_direct, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* pgsteal/s */
		lnappend(record_hdr->ust_time - svg_p->record_hdr->ust_time,
			 S_VALUE(spp->pgsteal, spc->pgsteal, itv),
			 out + 7, outsize + 7, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a, title, g_title, group, spmin, spmax,
				     out, outsize, svg_p, record_hdr);
	}
}
