/* register_types.cpp */

#include "register_types.h"
#include "class_db.h"
#include "wc/wc.h"

void register_mw_types() {

	ClassDB::register_class<WC>();
}

void unregister_mw_types() {
	//nothing to do here i guess
}