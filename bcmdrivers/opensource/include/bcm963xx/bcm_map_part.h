/*
<:label-BRCM:2012:DUAL/GPL:standard

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:> 
*/


#ifndef __BCM_MAP_PART_H
#define __BCM_MAP_PART_H

#if defined(CONFIG_BCM96318)
#include <6318_map_part.h>
#endif
#if defined(CONFIG_BCM963268)
#include <63268_map_part.h>
#endif
#if defined(CONFIG_BCM96328)
#include <6328_map_part.h>
#endif
#if defined(CONFIG_BCM96368)
#include <6368_map_part.h>
#endif
#if defined(CONFIG_BCM96816)
#include <6816_map_part.h>
#endif
#if defined(CONFIG_BCM96362)
#include <6362_map_part.h>
#endif

#endif

