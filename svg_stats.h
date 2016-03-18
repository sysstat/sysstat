/*
 * svg_stats.h: Include file used to display system statistics in SVG format.
 * (C) 2016 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _SVG_STATS_H
#define _SVG_STATS_H


/*
 ***************************************************************************
 * Prototypes for functions used to display system statistics in SVG.
 ***************************************************************************
 */

/* Functions used to display statistics in SVG */
__print_funct_t svg_print_cpu_stats
	(struct activity *, int, int, struct svg_parm *, unsigned long long,
	 struct record_header *);
__print_funct_t svg_print_pcsw_stats
	(struct activity *, int, int, struct svg_parm *, unsigned long long,
	 struct record_header *);
__print_funct_t svg_print_swap_stats
	(struct activity *, int, int, struct svg_parm *, unsigned long long,
	 struct record_header *);
__print_funct_t svg_print_paging_stats
	(struct activity *, int, int, struct svg_parm *, unsigned long long,
	 struct record_header *);
__print_funct_t svg_print_net_dev_stats
	(struct activity *, int, int, struct svg_parm *, unsigned long long,
	 struct record_header *);
	
#endif /* _SVG_STATS_H */
