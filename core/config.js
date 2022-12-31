
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function config_add(name,type,pdef,pflags,pfunc) {
	var def,flags,func;

	dprintf(0,"name: %s, type: %s, def: %s, flags: %x, func: %s\n", name, typestr(type), pdef, pflags, typeof(pfunc));
	if (typeof(pdef) == "undefined") 
}
