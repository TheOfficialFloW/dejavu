/* dejavu savestate plugin
 *
 * Copyright (C) 2020 TheFloW
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __STATE_H__
#define __STATE_H__

int save_state(const char *path);
int load_state(const char *path);

#endif
